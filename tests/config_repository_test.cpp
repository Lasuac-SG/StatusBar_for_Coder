#include "core/config_repository.h"
#include "core/internal/config_repository_operations.h"
#include "ui/widget_model.h"
#include "widgets/registry_setup.h"

#include <QtTest>

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTemporaryDir>
#include <QUuid>

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>

namespace {

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

class CurrentDirectoryGuard final {
public:
    CurrentDirectoryGuard()
        : original_(QDir::currentPath())
    {
    }

    ~CurrentDirectoryGuard() { QDir::setCurrent(original_); }

private:
    QString original_;
};

class ControlledLockOperations final : public Core::Internal::ConfigRepositoryOperations {
public:
    ControlledLockOperations()
        : delegate_(Core::Internal::defaultConfigRepositoryOperations())
    {
    }

    Core::Internal::LockAttemptResult attemptMigrationLock(
        QLockFile&,
        int) override
    {
        std::unique_lock lock(mutex_);
        ++lockAttempts_;
        stateChanged_.notify_all();
        if (lockAttempts_ == 2) {
            stateChanged_.wait(lock, [this] { return releaseSecondAttempt_; });
        }
        return {false, QLockFile::LockFailedError};
    }

    Core::Result<void> atomicWrite(const QString& path, const QByteArray& bytes) override
    {
        return delegate_->atomicWrite(path, bytes);
    }

    bool waitForLockAttempts(int expected)
    {
        using namespace std::chrono_literals;
        std::unique_lock lock(mutex_);
        return stateChanged_.wait_for(
            lock, 5s, [this, expected] { return lockAttempts_ >= expected; });
    }

    void releaseSecondAttempt()
    {
        std::lock_guard lock(mutex_);
        releaseSecondAttempt_ = true;
        stateChanged_.notify_all();
    }

    int lockAttempts() const
    {
        std::lock_guard lock(mutex_);
        return lockAttempts_;
    }

private:
    std::shared_ptr<Core::Internal::ConfigRepositoryOperations> delegate_;
    mutable std::mutex mutex_;
    std::condition_variable stateChanged_;
    int lockAttempts_{};
    bool releaseSecondAttempt_{};
};

class FailFirstDestinationWriteOperations final
    : public Core::Internal::ConfigRepositoryOperations {
public:
    explicit FailFirstDestinationWriteOperations(QString destination)
        : delegate_(Core::Internal::defaultConfigRepositoryOperations())
        , destination_(std::move(destination))
    {
    }

    Core::Internal::LockAttemptResult attemptMigrationLock(
        QLockFile& lock,
        int timeoutMs) override
    {
        return delegate_->attemptMigrationLock(lock, timeoutMs);
    }

    Core::Result<void> atomicWrite(const QString& path, const QByteArray& bytes) override
    {
        if (path == destination_) {
            ++destinationWriteAttempts_;
            if (destinationWriteAttempts_ == 1) {
                return Core::Result<void>::failure(
                    QStringLiteral("Injected destination write failure"));
            }
        } else {
            ++backupWriteAttempts_;
        }
        return delegate_->atomicWrite(path, bytes);
    }

    int destinationWriteAttempts() const noexcept { return destinationWriteAttempts_; }
    int backupWriteAttempts() const noexcept { return backupWriteAttempts_; }

private:
    std::shared_ptr<Core::Internal::ConfigRepositoryOperations> delegate_;
    QString destination_;
    int destinationWriteAttempts_{};
    int backupWriteAttempts_{};
};

} // namespace

class ConfigRepositoryTest final : public QObject {
    Q_OBJECT

private slots:
    void migratesLegacyAndCreatesBackup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        const QByteArray legacyBytes = R"({ "widgets": [{"name":"Clock","slot":2}] })";
        QVERIFY(writeBytes(legacy, legacyBytes));

        const Core::ConfigRepository repository(destination, {legacy});
        const auto result = repository.load();

        QVERIFY2(result.hasValue(), qPrintable(result.error()));
        QCOMPARE(result.value().version, 1);
        QCOMPARE(result.value().widgets.size(), 1);
        const auto& widget = result.value().widgets.front();
        QVERIFY(!widget.id.isEmpty());
        QCOMPARE(QUuid(widget.id).toString(QUuid::WithoutBraces), widget.id);
        QCOMPARE(widget.type, QString("Clock"));
        QCOMPARE(widget.slot, 2);
        QVERIFY(widget.settings.isEmpty());

        QCOMPARE(readBytes(legacy), legacyBytes);
        QCOMPARE(readBytes(directory.filePath("config.legacy.backup.json")), legacyBytes);

        QJsonParseError parseError;
        const auto migrated = QJsonDocument::fromJson(readBytes(destination), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QCOMPARE(migrated.object().value("version").toInt(), 1);
        QCOMPARE(migrated.object().value("widgets").toArray().size(), 1);
    }

    void leavesMalformedSourceUntouched()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        const QByteArray malformedBytes = R"({"widgets":[{"name":"Clock","slot":})";
        QVERIFY(writeBytes(legacy, malformedBytes));

        const Core::ConfigRepository repository(destination, {legacy});
        const auto result = repository.load();

        QVERIFY(!result.hasValue());
        QVERIFY(!result.error().isEmpty());
        QVERIFY(!QFile::exists(destination));
        QCOMPARE(readBytes(legacy), malformedBytes);
        QVERIFY(!QFile::exists(directory.filePath("config.legacy.backup.json")));
    }

    void atomicallyRoundTripsVersionOne()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("nested/config.json");
        const Core::ConfigRepository repository(destination);
        const Core::ConfigDocument expected{
            1,
            {
                {"clock-id", "Clock", 2, QJsonObject{{"timezone", "UTC"}}},
                {"cpu-id", "Cpu", 5, QJsonObject{{"intervalMs", 1000}}},
            },
        };

        const auto saveResult = repository.save(expected);
        QVERIFY2(saveResult.hasValue(), qPrintable(saveResult.error()));
        QCOMPARE(repository.path(), destination);

        const auto loadResult = repository.load();
        QVERIFY2(loadResult.hasValue(), qPrintable(loadResult.error()));
        QCOMPARE(loadResult.value().version, expected.version);
        QCOMPARE(loadResult.value().widgets, expected.widgets);

        const QByteArray validBytes = readBytes(destination);
        Core::ConfigDocument invalid = expected;
        invalid.widgets.append(expected.widgets.front());
        const auto rejectedSave = repository.save(invalid);
        QVERIFY(!rejectedSave.hasValue());
        QCOMPARE(readBytes(destination), validBytes);
    }

    void internalFactoryFallsBackForNullOperations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const auto repository = Core::Internal::makeConfigRepository(
            destination, {}, nullptr);
        const Core::ConfigDocument expected{
            1,
            {{"clock-id", "Clock", 2, QJsonObject{{"timezone", "UTC"}}}},
        };

        const auto saved = repository.save(expected);
        QVERIFY2(saved.hasValue(), qPrintable(saved.error()));
        const auto loaded = repository.load();
        QVERIFY2(loaded.hasValue(), qPrintable(loaded.error()));
        QCOMPARE(loaded.value().widgets, expected.widgets);
    }

    void preservesUnknownWidgetEntries()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QJsonObject unknownSettings{
            {"provider", "future"},
            {"options", QJsonObject{{"accent", "violet"}}},
        };
        const QJsonObject root{
            {"version", 1},
            {"widgets", QJsonArray{QJsonObject{
                            {"id", "future-widget"},
                            {"type", "WidgetFromTheFuture"},
                            {"slot", 7},
                            {"settings", unknownSettings},
                        }}},
        };
        QVERIFY(writeBytes(destination, QJsonDocument(root).toJson(QJsonDocument::Compact)));

        const Core::ConfigRepository repository(destination);
        const auto loaded = repository.load();
        QVERIFY2(loaded.hasValue(), qPrintable(loaded.error()));
        QCOMPARE(loaded.value().widgets.size(), 1);
        QCOMPARE(loaded.value().widgets.front().type, QString("WidgetFromTheFuture"));
        QCOMPARE(loaded.value().widgets.front().settings, unknownSettings);

        const auto saved = repository.save(loaded.value());
        QVERIFY2(saved.hasValue(), qPrintable(saved.error()));
        const auto reloaded = repository.load();
        QVERIFY2(reloaded.hasValue(), qPrintable(reloaded.error()));
        QCOMPARE(reloaded.value().widgets, loaded.value().widgets);
    }

    void rejectsDuplicateIdsAndInvalidSlots()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const Core::ConfigRepository repository(destination);
        const auto widget = [](const QString& id, int slot) {
            return QJsonObject{
                {"id", id},
                {"type", "Clock"},
                {"slot", slot},
                {"settings", QJsonObject{}},
            };
        };

        const QJsonObject duplicateIds{
            {"version", 1},
            {"widgets", QJsonArray{widget("duplicate", 0), widget("duplicate", 2)}},
        };
        QVERIFY(writeBytes(destination, QJsonDocument(duplicateIds).toJson()));
        const auto duplicateResult = repository.load();
        QVERIFY(!duplicateResult.hasValue());
        QVERIFY2(duplicateResult.error().contains("1"), qPrintable(duplicateResult.error()));
        QVERIFY2(duplicateResult.error().contains("id", Qt::CaseInsensitive),
                 qPrintable(duplicateResult.error()));

        const QJsonObject invalidSlot{
            {"version", 1},
            {"widgets", QJsonArray{widget("clock", -1)}},
        };
        QVERIFY(writeBytes(destination, QJsonDocument(invalidSlot).toJson()));
        const auto slotResult = repository.load();
        QVERIFY(!slotResult.hasValue());
        QVERIFY(!slotResult.error().isEmpty());
    }

    void refusesToOverwriteConflictingBackup_data()
    {
        QTest::addColumn<QByteArray>("legacyBytes");

        QTest::newRow("different valid legacy")
            << QByteArray(R"({"widgets":[{"name":"Cpu","slot":3}]})");
        QTest::newRow("different malformed legacy")
            << QByteArray(R"({"widgets":[{"name":"Cpu","slot":})");
    }

    void refusesToOverwriteConflictingBackup()
    {
        QFETCH(QByteArray, legacyBytes);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        const QString backup = directory.filePath("config.legacy.backup.json");
        const QByteArray originalBackup =
            R"({"widgets":[{"name":"Clock","slot":1}]})";
        QVERIFY(writeBytes(legacy, legacyBytes));
        QVERIFY(writeBytes(backup, originalBackup));

        const Core::ConfigRepository repository(destination, {legacy});
        const auto result = repository.load();

        QVERIFY(!result.hasValue());
        QVERIFY2(result.error().contains("backup", Qt::CaseInsensitive),
                 qPrintable(result.error()));
        QCOMPARE(readBytes(legacy), legacyBytes);
        QCOMPARE(readBytes(backup), originalBackup);
        QVERIFY(!QFile::exists(destination));
    }

    void reusesIdenticalExistingBackup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        const QString backup = directory.filePath("config.legacy.backup.json");
        const QByteArray legacyBytes = R"({"widgets":[{"name":"Clock","slot":5}]})";
        QVERIFY(writeBytes(legacy, legacyBytes));
        QVERIFY(writeBytes(backup, legacyBytes));

        const Core::ConfigRepository repository(destination, {legacy});
        const auto result = repository.load();

        QVERIFY2(result.hasValue(), qPrintable(result.error()));
        QCOMPARE(result.value().widgets.front().slot, 5);
        QCOMPARE(readBytes(backup), legacyBytes);
        QCOMPARE(readBytes(legacy), legacyBytes);
        QVERIFY(QFile::exists(destination));
    }

    void respectsHeldMigrationLock()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        QVERIFY(writeBytes(legacy, R"({"widgets":[{"name":"Clock","slot":2}]})"));

        QLockFile heldLock(destination + ".migration.lock");
        heldLock.setStaleLockTime(30'000);
        QVERIFY(heldLock.tryLock(0));

        const Core::ConfigRepository repository(destination, {legacy});
        const auto result = repository.load();

        QVERIFY(!result.hasValue());
        QVERIFY2(result.error().contains("lock", Qt::CaseInsensitive),
                 qPrintable(result.error()));
        QVERIFY(!QFile::exists(destination));
        QVERIFY(!QFile::exists(directory.filePath("config.legacy.backup.json")));
    }

    void loadsDestinationCreatedAfterInitialLockTimeout()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        QVERIFY(writeBytes(legacy, R"({"widgets":[{"name":"Clock","slot":2}]})"));

        auto operations = std::make_shared<ControlledLockOperations>();
        const auto repository = Core::Internal::makeConfigRepository(
            destination, {legacy}, operations);
        auto pendingLoad = std::async(std::launch::async, [&repository] {
            return repository.load();
        });
        const bool reachedSecondAttempt = operations->waitForLockAttempts(2);
        const int lockAttempts = operations->lockAttempts();

        const QJsonObject concurrentDestination{
            {"version", 1},
            {"widgets", QJsonArray{QJsonObject{
                            {"id", "created-concurrently"},
                            {"type", "WidgetFromTheFuture"},
                            {"slot", 9},
                            {"settings", QJsonObject{{"source", "other-instance"}}},
                        }}},
        };
        const bool destinationWritten = reachedSecondAttempt && writeBytes(
            destination, QJsonDocument(concurrentDestination).toJson(QJsonDocument::Compact));
        operations->releaseSecondAttempt();

        QVERIFY2(reachedSecondAttempt,
                 "repository did not enter its second lock attempt");
        QCOMPARE(lockAttempts, 2);
        QVERIFY(destinationWritten);

        const auto result = pendingLoad.get();
        QVERIFY2(result.hasValue(), qPrintable(result.error()));
        QCOMPARE(result.value().widgets.front().id, QString("created-concurrently"));
        QCOMPARE(operations->lockAttempts(), 2);
        QVERIFY(!QFile::exists(directory.filePath("config.legacy.backup.json")));
        QVERIFY(QFile::exists(legacy));
    }

    void recoversAfterDestinationFailureFollowingBackupCommit()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        const QString backup = directory.filePath("config.legacy.backup.json");
        const QByteArray legacyBytes = R"({"widgets":[{"name":"Clock","slot":2}]})";
        QVERIFY(writeBytes(legacy, legacyBytes));

        auto operations = std::make_shared<FailFirstDestinationWriteOperations>(destination);
        const auto repository = Core::Internal::makeConfigRepository(
            destination, {legacy}, operations);

        const auto failed = repository.load();
        QVERIFY(!failed.hasValue());
        QVERIFY2(failed.error().contains("Injected"), qPrintable(failed.error()));
        QCOMPARE(readBytes(legacy), legacyBytes);
        QCOMPARE(readBytes(backup), legacyBytes);
        QVERIFY(!QFileInfo::exists(destination));
        QCOMPARE(operations->destinationWriteAttempts(), 1);
        QCOMPARE(operations->backupWriteAttempts(), 1);

        const auto retried = repository.load();
        QVERIFY2(retried.hasValue(), qPrintable(retried.error()));
        QCOMPARE(retried.value().widgets.size(), 1);
        QCOMPARE(readBytes(legacy), legacyBytes);
        QCOMPARE(readBytes(backup), legacyBytes);
        QVERIFY(QFileInfo(destination).isFile());
        QCOMPARE(operations->destinationWriteAttempts(), 2);
        QCOMPARE(operations->backupWriteAttempts(), 1);
    }

    void usesFirstExistingLegacyCandidate()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString first = directory.filePath("first.json");
        const QString second = directory.filePath("second.json");
        QVERIFY(writeBytes(first, R"({"widgets":[{"name":"Clock","slot":1}]})"));
        QVERIFY(writeBytes(second, R"({"widgets":[{"name":"Cpu","slot":6}]})"));

        const Core::ConfigRepository repository(destination, {first, second});
        const auto result = repository.load();

        QVERIFY2(result.hasValue(), qPrintable(result.error()));
        QCOMPARE(result.value().widgets.size(), 1);
        QCOMPARE(result.value().widgets.front().type, QString("Clock"));
        QCOMPARE(result.value().widgets.front().slot, 1);
    }

    void excludesDestinationAliasesAndDeduplicatesCandidates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString first = directory.filePath("first.json");
        const QString second = directory.filePath("second.json");
        QVERIFY(writeBytes(first, R"({"widgets":[{"name":"Clock","slot":3}]})"));
        QVERIFY(writeBytes(second, R"({"widgets":[{"name":"Cpu","slot":7}]})"));

        const Core::ConfigRepository repository(
            destination,
            {destination,
             directory.filePath("./config.json"),
             first,
             directory.filePath("./first.json"),
             second});
        const auto result = repository.load();

        QVERIFY2(result.hasValue(), qPrintable(result.error()));
        QCOMPARE(result.value().widgets.front().type, QString("Clock"));
        QCOMPARE(result.value().widgets.front().slot, 3);
    }

    void freezesRelativePathsAtConstruction()
    {
        QTemporaryDir sourceDirectory;
        QTemporaryDir otherDirectory;
        QVERIFY(sourceDirectory.isValid());
        QVERIFY(otherDirectory.isValid());
        CurrentDirectoryGuard restoreCurrentDirectory;
        QVERIFY(QDir::setCurrent(sourceDirectory.path()));
        QVERIFY(QDir().mkpath("nested"));
        QVERIFY(writeBytes("legacy.json", R"({"widgets":[{"name":"Clock","slot":4}]})"));

        const Core::ConfigRepository repository("nested/config.json", {"legacy.json"});
        const QString expectedDestination =
            QDir::cleanPath(sourceDirectory.filePath("nested/config.json"));
        QVERIFY(QDir::setCurrent(otherDirectory.path()));

        const auto result = repository.load();

        QVERIFY2(result.hasValue(), qPrintable(result.error()));
        QCOMPARE(repository.path(), expectedDestination);
        QCOMPARE(result.value().widgets.front().slot, 4);
        QVERIFY(QFile::exists(expectedDestination));
        QVERIFY(!QFile::exists(otherDirectory.filePath("nested/config.json")));
    }

    void reportsDeterministicIoFailures()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Core::ConfigDocument document{
            1,
            {{"clock", "Clock", 0, QJsonObject{}}},
        };

        const QString parentFile = directory.filePath("ordinary-file");
        QVERIFY(writeBytes(parentFile, "not a directory"));
        const Core::ConfigRepository openFailure(parentFile + "/config.json");
        const auto openResult = openFailure.save(document);
        QVERIFY(!openResult.hasValue());
        QVERIFY(!openResult.error().isEmpty());

        const QString directoryDestination = directory.filePath("existing-directory");
        QVERIFY(QDir().mkpath(directoryDestination));
        const Core::ConfigRepository commitFailure(directoryDestination);
        const auto commitResult = commitFailure.save(document);
        QVERIFY(!commitResult.hasValue());
        QVERIFY(!commitResult.error().isEmpty());
        QVERIFY(QFileInfo(directoryDestination).isDir());
    }

    void reportsSpecificVersionOneField_data()
    {
        QTest::addColumn<QJsonObject>("widget");
        QTest::addColumn<QString>("field");

        const auto valid = [] {
            return QJsonObject{
                {"id", "clock"},
                {"type", "Clock"},
                {"slot", 0},
                {"settings", QJsonObject{}},
            };
        };

        auto invalidId = valid();
        invalidId.insert("id", QJsonValue::Null);
        QTest::newRow("id") << invalidId << QString("id");
        auto invalidType = valid();
        invalidType.insert("type", "");
        QTest::newRow("type") << invalidType << QString("type");
        auto invalidSlot = valid();
        invalidSlot.insert("slot", -1);
        QTest::newRow("slot") << invalidSlot << QString("slot");
        auto invalidSettings = valid();
        invalidSettings.insert("settings", QJsonArray{});
        QTest::newRow("settings") << invalidSettings << QString("settings");
    }

    void reportsSpecificVersionOneField()
    {
        QFETCH(QJsonObject, widget);
        QFETCH(QString, field);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QJsonObject root{
            {"version", 1},
            {"widgets", QJsonArray{widget}},
        };
        QVERIFY(writeBytes(destination, QJsonDocument(root).toJson()));

        const Core::ConfigRepository repository(destination);
        const auto result = repository.load();

        QVERIFY(!result.hasValue());
        QVERIFY2(result.error().contains("0"), qPrintable(result.error()));
        QVERIFY2(result.error().contains(field, Qt::CaseInsensitive),
                 qPrintable(result.error()));
    }

    void reportsSpecificLegacyField_data()
    {
        QTest::addColumn<QJsonObject>("widget");
        QTest::addColumn<QString>("field");

        QTest::newRow("name")
            << QJsonObject{{"name", QJsonValue::Null}, {"slot", 0}} << QString("name");
        QTest::newRow("slot")
            << QJsonObject{{"name", "Clock"}, {"slot", -1}} << QString("slot");
    }

    void reportsSpecificLegacyField()
    {
        QFETCH(QJsonObject, widget);
        QFETCH(QString, field);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        const QString legacy = directory.filePath("legacy.json");
        const QJsonObject root{{"widgets", QJsonArray{widget}}};
        QVERIFY(writeBytes(legacy, QJsonDocument(root).toJson()));

        const Core::ConfigRepository repository(destination, {legacy});
        const auto result = repository.load();

        QVERIFY(!result.hasValue());
        QVERIFY2(result.error().contains("0"), qPrintable(result.error()));
        QVERIFY2(result.error().contains(field, Qt::CaseInsensitive),
                 qPrintable(result.error()));
    }

    void resultTemporaryAccessReturnsOwnedValues()
    {
        static_assert(std::is_same_v<
                      decltype(std::declval<const Core::Result<int>&>().error()),
                      const QString&>);
        static_assert(std::is_same_v<
                      decltype(std::declval<const Core::Result<void>&>().error()),
                      const QString&>);
        static_assert(std::is_same_v<
                      decltype(Core::Result<int>::failure(QStringLiteral("error")).error()),
                      QString>);
        static_assert(std::is_same_v<
                      decltype(Core::Result<void>::failure(QStringLiteral("error")).error()),
                      QString>);
        static_assert(std::is_same_v<
                      decltype(Core::Result<QString>::success(QStringLiteral("value")).value()),
                      QString>);
        static_assert(std::is_same_v<
                      decltype(std::move(std::declval<const Core::Result<int>&>()).error()),
                      QString>);
        static_assert(std::is_same_v<
                      decltype(std::move(std::declval<const Core::Result<QString>&>()).value()),
                      QString>);
        static_assert(std::is_same_v<
                      decltype(std::move(std::declval<const Core::Result<void>&>()).error()),
                      QString>);

        QCOMPARE(Core::Result<int>::failure(QStringLiteral("temporary error")).error(),
                 QString("temporary error"));
        QCOMPARE(Core::Result<QString>::success(QStringLiteral("temporary value")).value(),
                 QString("temporary value"));
    }

    void widgetModelPreservesUnknownEntryWhenKnownWidgetMoves()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Widgets::RegisterAllWidgets();

        const QString destination = directory.filePath("config.json");
        const Core::WidgetConfig unknown{
            "future-id",
            "WidgetFromTheFuture",
            8,
            QJsonObject{{"provider", "future"}, {"nested", QJsonObject{{"keep", true}}}},
        };
        const Core::WidgetConfig known{
            "clock-id",
            "Clock",
            0,
            QJsonObject{{"timezone", "UTC"}},
        };
        Core::ConfigRepository repository(destination);
        const auto initialSave = repository.save(Core::ConfigDocument{1, {unknown, known}});
        QVERIFY2(initialSave.hasValue(), qPrintable(initialSave.error()));

        UI::WidgetModel model(repository);
        const auto loadResult = model.loadFromConfig();
        QVERIFY2(loadResult.hasValue(), qPrintable(loadResult.error()));
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.handleWidgetDropped(0, 55.0F, 10.0F, 0.0F, 100.0F));

        const auto reloaded = repository.load();
        QVERIFY2(reloaded.hasValue(), qPrintable(reloaded.error()));
        QCOMPARE(reloaded.value().widgets.size(), 2);
        QVERIFY(reloaded.value().widgets.at(0) == unknown);
        QCOMPARE(reloaded.value().widgets.at(1).id, known.id);
        QCOMPARE(reloaded.value().widgets.at(1).type, known.type);
        QCOMPARE(reloaded.value().widgets.at(1).slot, 4);
        QCOMPARE(reloaded.value().widgets.at(1).settings, known.settings);
    }

    void widgetModelRollsBackMoveWhenSaveFails()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Widgets::RegisterAllWidgets();

        const QString destination = directory.filePath("config.json");
        Core::ConfigRepository repository(destination);
        const Core::ConfigDocument document{
            1,
            {
                {"clock-id", "Clock", 0, QJsonObject{{"timezone", "UTC"}}},
                {"cpu-id", "Cpu", 5, {}},
            },
        };
        const auto initialSave = repository.save(document);
        QVERIFY2(initialSave.hasValue(), qPrintable(initialSave.error()));

        UI::WidgetModel model(repository);
        const auto loadResult = model.loadFromConfig();
        QVERIFY2(loadResult.hasValue(), qPrintable(loadResult.error()));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 5);
        QSignalSpy persistenceErrors(&model, &UI::WidgetModel::persistenceError);
        QSignalSpy changedRows(&model, &QAbstractItemModel::dataChanged);

        QVERIFY(QFile::remove(destination));
        QVERIFY(QDir().mkpath(destination));
        QVERIFY(!model.handleWidgetDropped(0, 65.0F, 10.0F, 0.0F, 90.0F));

        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 5);
        QVERIFY2(!model.lastError().isEmpty(), "save failure must be exposed");
        QCOMPARE(persistenceErrors.count(), 1);
        QCOMPARE(persistenceErrors.front().front().toString(), model.lastError());
        QCOMPARE(changedRows.count(), 0);
        QVERIFY(QFileInfo(destination).isDir());
    }

    void widgetModelRejectsMultiWidgetCollisionWithoutPersistence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Widgets::RegisterAllWidgets();

        const QString destination = directory.filePath("config.json");
        Core::ConfigRepository repository(destination);
        const Core::ConfigDocument document{
            1,
            {
                {"wide", "Clock", 0, {}},
                {"first", "Cpu", 4, {}},
                {"second", "Cpu", 6, {}},
            },
        };
        const auto initialSave = repository.save(document);
        QVERIFY2(initialSave.hasValue(), qPrintable(initialSave.error()));
        const QByteArray originalBytes = readBytes(destination);

        UI::WidgetModel model(repository);
        const auto loadResult = model.loadFromConfig();
        QVERIFY2(loadResult.hasValue(), qPrintable(loadResult.error()));
        QSignalSpy changedRows(&model, &QAbstractItemModel::dataChanged);

        QVERIFY(!model.handleWidgetDropped(0, 55.0F, 10.0F, 0.0F, 90.0F));

        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 4);
        QCOMPARE(model.data(model.index(2), UI::WidgetModel::SlotRole).toInt(), 6);
        QCOMPARE(changedRows.count(), 0);
        QCOMPARE(readBytes(destination), originalBytes);
        QVERIFY2(model.lastError().contains("reject", Qt::CaseInsensitive),
                 qPrintable(model.lastError()));
    }

    void widgetModelRejectsDropFromInvalidExistingLayout()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Widgets::RegisterAllWidgets();

        const QString destination = directory.filePath("config.json");
        Core::ConfigRepository repository(destination);
        const Core::ConfigDocument document{
            1,
            {
                {"clock", "Clock", 0, {}},
                {"cpu", "Cpu", 2, {}},
            },
        };
        const auto initialSave = repository.save(document);
        QVERIFY2(initialSave.hasValue(), qPrintable(initialSave.error()));
        const QByteArray originalBytes = readBytes(destination);

        UI::WidgetModel model(repository);
        const auto loadResult = model.loadFromConfig();
        QVERIFY2(loadResult.hasValue(), qPrintable(loadResult.error()));

        QVERIFY(!model.handleWidgetDropped(0, 65.0F, 10.0F, 0.0F, 90.0F));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 2);
        QCOMPARE(readBytes(destination), originalBytes);
    }

    void widgetModelReportsLoadFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString destination = directory.filePath("config.json");
        QVERIFY(writeBytes(destination, R"({"version":1,"widgets":[})"));

        UI::WidgetModel model{Core::ConfigRepository(destination)};
        const auto result = model.loadFromConfig();

        QVERIFY(!result.hasValue());
        QVERIFY(!result.error().isEmpty());
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(ConfigRepositoryTest)
#include "config_repository_test.moc"
