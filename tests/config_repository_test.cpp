#include "core/config_repository.h"

#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>

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
        const QString backup = directory.filePath("config.legacy.backup.json");
        if (QFile::exists(backup)) {
            QCOMPARE(readBytes(backup), malformedBytes);
        }
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
        QVERIFY(!duplicateResult.error().isEmpty());

        const QJsonObject invalidSlot{
            {"version", 1},
            {"widgets", QJsonArray{widget("clock", -1)}},
        };
        QVERIFY(writeBytes(destination, QJsonDocument(invalidSlot).toJson()));
        const auto slotResult = repository.load();
        QVERIFY(!slotResult.hasValue());
        QVERIFY(!slotResult.error().isEmpty());
    }
};

QTEST_GUILESS_MAIN(ConfigRepositoryTest)
#include "config_repository_test.moc"
