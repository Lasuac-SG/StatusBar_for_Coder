#include "core/config_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Core {
namespace {

Result<QByteArray> readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Result<QByteArray>::failure(
            QStringLiteral("Cannot open %1: %2").arg(path, file.errorString()));
    }

    QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return Result<QByteArray>::failure(
            QStringLiteral("Cannot completely read %1: %2").arg(path, file.errorString()));
    }
    return Result<QByteArray>::success(std::move(bytes));
}

Result<void> ensureParentDirectory(const QString& path)
{
    const QString parent = QFileInfo(path).absolutePath();
    QDir directory;
    if (!directory.mkpath(parent)) {
        return Result<void>::failure(
            QStringLiteral("Cannot create configuration directory: %1").arg(parent));
    }
    return Result<void>::success();
}

Result<void> atomicallyWrite(const QString& path, const QByteArray& bytes)
{
    const auto directoryResult = ensureParentDirectory(path);
    if (!directoryResult.hasValue()) {
        return directoryResult;
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        return Result<void>::failure(
            QStringLiteral("Cannot open %1 for writing: %2").arg(path, file.errorString()));
    }
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return Result<void>::failure(
            QStringLiteral("Cannot completely write %1: %2").arg(path, file.errorString()));
    }
    if (!file.commit()) {
        return Result<void>::failure(
            QStringLiteral("Cannot commit %1: %2").arg(path, file.errorString()));
    }
    return Result<void>::success();
}

bool readNonNegativeInteger(const QJsonValue& value, int& output)
{
    if (!value.isDouble()) {
        return false;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number || number < 0.0
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }

    output = static_cast<int>(number);
    return true;
}

Result<void> validate(const ConfigDocument& document)
{
    if (document.version != 1) {
        return Result<void>::failure(QStringLiteral("Unsupported configuration version"));
    }

    QSet<QString> ids;
    for (qsizetype index = 0; index < document.widgets.size(); ++index) {
        const auto& widget = document.widgets.at(index);
        if (widget.id.isEmpty()) {
            return Result<void>::failure(
                QStringLiteral("Widget %1 field 'id' must be nonempty").arg(index));
        }
        if (ids.contains(widget.id)) {
            return Result<void>::failure(
                QStringLiteral("Widget %1 field 'id' duplicates '%2'").arg(index).arg(widget.id));
        }
        if (widget.type.isEmpty()) {
            return Result<void>::failure(
                QStringLiteral("Widget %1 field 'type' must be nonempty").arg(index));
        }
        if (widget.slot < 0) {
            return Result<void>::failure(
                QStringLiteral("Widget %1 field 'slot' must be a nonnegative integer").arg(index));
        }
        ids.insert(widget.id);
    }

    return Result<void>::success();
}

Result<ConfigDocument> parseVersionOne(const QByteArray& bytes)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Malformed configuration JSON: %1").arg(parseError.errorString()));
    }
    if (!json.isObject()) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Configuration root must be an object"));
    }

    const QJsonObject root = json.object();
    const QJsonValue versionValue = root.value(QStringLiteral("version"));
    if (!versionValue.isDouble() || versionValue.toDouble() != 1.0) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Configuration version must be exactly 1"));
    }

    const QJsonValue widgetsValue = root.value(QStringLiteral("widgets"));
    if (!widgetsValue.isArray()) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Configuration widgets must be an array"));
    }

    ConfigDocument document;
    const QJsonArray widgets = widgetsValue.toArray();
    document.widgets.reserve(widgets.size());
    for (qsizetype index = 0; index < widgets.size(); ++index) {
        const QJsonValue entryValue = widgets.at(index);
        if (!entryValue.isObject()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 must be an object").arg(index));
        }

        const QJsonObject entry = entryValue.toObject();
        const QJsonValue idValue = entry.value(QStringLiteral("id"));
        const QJsonValue typeValue = entry.value(QStringLiteral("type"));
        const QJsonValue slotValue = entry.value(QStringLiteral("slot"));
        const QJsonValue settingsValue = entry.value(QStringLiteral("settings"));
        int slot = 0;
        if (!idValue.isString()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 field 'id' must be a string").arg(index));
        }
        if (idValue.toString().isEmpty()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 field 'id' must be nonempty").arg(index));
        }
        if (!typeValue.isString()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 field 'type' must be a string").arg(index));
        }
        if (typeValue.toString().isEmpty()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 field 'type' must be nonempty").arg(index));
        }
        if (!readNonNegativeInteger(slotValue, slot)) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 field 'slot' must be a nonnegative integer").arg(index));
        }
        if (!settingsValue.isObject()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Widget %1 field 'settings' must be an object").arg(index));
        }

        document.widgets.append(
            WidgetConfig{idValue.toString(), typeValue.toString(), slot, settingsValue.toObject()});
    }

    const auto validation = validate(document);
    if (!validation.hasValue()) {
        return Result<ConfigDocument>::failure(validation.error());
    }
    return Result<ConfigDocument>::success(std::move(document));
}

Result<ConfigDocument> parseLegacy(const QByteArray& bytes)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Malformed legacy configuration JSON: %1")
                .arg(parseError.errorString()));
    }
    if (!json.isObject()) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Legacy configuration root must be an object"));
    }

    const QJsonValue widgetsValue = json.object().value(QStringLiteral("widgets"));
    if (!widgetsValue.isArray()) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Legacy configuration widgets must be an array"));
    }

    ConfigDocument document;
    const QJsonArray widgets = widgetsValue.toArray();
    document.widgets.reserve(widgets.size());
    for (qsizetype index = 0; index < widgets.size(); ++index) {
        const QJsonValue entryValue = widgets.at(index);
        if (!entryValue.isObject()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Legacy widget %1 must be an object").arg(index));
        }

        const QJsonObject entry = entryValue.toObject();
        const QJsonValue nameValue = entry.value(QStringLiteral("name"));
        int slot = 0;
        if (!nameValue.isString()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Legacy widget %1 field 'name' must be a string").arg(index));
        }
        if (nameValue.toString().isEmpty()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Legacy widget %1 field 'name' must be nonempty").arg(index));
        }
        if (!readNonNegativeInteger(entry.value(QStringLiteral("slot")), slot)) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Legacy widget %1 field 'slot' must be a nonnegative integer")
                    .arg(index));
        }

        document.widgets.append(WidgetConfig{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            nameValue.toString(),
            slot,
            {},
        });
    }

    const auto validation = validate(document);
    if (!validation.hasValue()) {
        return Result<ConfigDocument>::failure(validation.error());
    }
    return Result<ConfigDocument>::success(std::move(document));
}

QByteArray serialize(const ConfigDocument& document)
{
    QJsonArray widgets;
    for (const auto& widget : document.widgets) {
        widgets.append(QJsonObject{
            {QStringLiteral("id"), widget.id},
            {QStringLiteral("type"), widget.type},
            {QStringLiteral("slot"), widget.slot},
            {QStringLiteral("settings"), widget.settings},
        });
    }

    return QJsonDocument(QJsonObject{
                             {QStringLiteral("version"), document.version},
                             {QStringLiteral("widgets"), widgets},
                         })
        .toJson(QJsonDocument::Indented);
}

QString cleanAbsolutePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool pathsMatch(const QString& left, const QString& right)
{
#ifdef Q_OS_WIN
    return left.compare(right, Qt::CaseInsensitive) == 0;
#else
    return left == right;
#endif
}

} // namespace

ConfigRepository::ConfigRepository(QString configPath, QStringList legacyCandidates)
    : configPath_(cleanAbsolutePath(configPath))
{
    for (const QString& candidate : legacyCandidates) {
        if (candidate.isEmpty()) {
            continue;
        }

        const QString frozenCandidate = cleanAbsolutePath(candidate);
        if (pathsMatch(frozenCandidate, configPath_)) {
            continue;
        }

        const bool duplicate = std::any_of(
            legacyCandidates_.cbegin(),
            legacyCandidates_.cend(),
            [&frozenCandidate](const QString& existing) {
                return pathsMatch(existing, frozenCandidate);
            });
        if (!duplicate) {
            legacyCandidates_.append(frozenCandidate);
        }
    }
}

Result<ConfigDocument> ConfigRepository::load() const
{
    const auto loadDestination = [this]() -> Result<ConfigDocument> {
        const auto bytes = readAll(configPath_);
        if (!bytes.hasValue()) {
            return Result<ConfigDocument>::failure(bytes.error());
        }
        return parseVersionOne(bytes.value());
    };

    if (QFileInfo::exists(configPath_)) {
        return loadDestination();
    }

    const auto directoryResult = ensureParentDirectory(configPath_);
    if (!directoryResult.hasValue()) {
        return Result<ConfigDocument>::failure(directoryResult.error());
    }

    const QString migrationLockPath = configPath_ + QStringLiteral(".migration.lock");
    QLockFile migrationLock(migrationLockPath);
    migrationLock.setStaleLockTime(30'000);
    constexpr int migrationLockAttemptMs = 500;
    constexpr int migrationLockAttempts = 2;
    bool migrationLockAcquired = false;
    for (int attempt = 0; attempt < migrationLockAttempts; ++attempt) {
        if (migrationLock.tryLock(migrationLockAttemptMs)) {
            migrationLockAcquired = true;
            break;
        }

        switch (migrationLock.error()) {
        case QLockFile::LockFailedError:
            if (QFileInfo::exists(configPath_)) {
                return loadDestination();
            }
            if (attempt + 1 == migrationLockAttempts) {
                return Result<ConfigDocument>::failure(
                    QStringLiteral("Timed out after %1 ms waiting for configuration migration "
                                   "lock: %2")
                        .arg(migrationLockAttemptMs * migrationLockAttempts)
                        .arg(migrationLockPath));
            }
            break;
        case QLockFile::PermissionError:
            return Result<ConfigDocument>::failure(
                QStringLiteral("Permission denied while acquiring configuration migration lock: "
                               "%1")
                    .arg(migrationLockPath));
        case QLockFile::UnknownError:
            return Result<ConfigDocument>::failure(
                QStringLiteral("Unknown error while acquiring configuration migration lock: %1")
                    .arg(migrationLockPath));
        case QLockFile::NoError:
            return Result<ConfigDocument>::failure(
                QStringLiteral("Configuration migration lock failed without an error: %1")
                    .arg(migrationLockPath));
        }
    }

    if (!migrationLockAcquired) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Cannot acquire configuration migration lock: %1")
                .arg(migrationLockPath));
    }

    if (QFileInfo::exists(configPath_)) {
        return loadDestination();
    }

    QString legacyPath;
    for (const QString& candidate : legacyCandidates_) {
        if (QFileInfo(candidate).isFile()) {
            legacyPath = candidate;
            break;
        }
    }

    if (legacyPath.isEmpty()) {
        return Result<ConfigDocument>::success(ConfigDocument{});
    }

    const auto legacyBytes = readAll(legacyPath);
    if (!legacyBytes.hasValue()) {
        return Result<ConfigDocument>::failure(legacyBytes.error());
    }

    const QString backupPath =
        QFileInfo(configPath_).dir().filePath(QStringLiteral("config.legacy.backup.json"));
    if (pathsMatch(backupPath, configPath_)) {
        return Result<ConfigDocument>::failure(
            QStringLiteral("Configuration destination conflicts with legacy backup path"));
    }
    if (QFileInfo::exists(backupPath)) {
        const auto backupBytes = readAll(backupPath);
        if (!backupBytes.hasValue()) {
            return Result<ConfigDocument>::failure(backupBytes.error());
        }
        if (backupBytes.value() != legacyBytes.value()) {
            return Result<ConfigDocument>::failure(
                QStringLiteral("Legacy backup conflict at %1; existing bytes differ from %2")
                    .arg(backupPath, legacyPath));
        }
    }

    auto migrated = parseLegacy(legacyBytes.value());
    if (!migrated.hasValue()) {
        return migrated;
    }

    if (!QFileInfo::exists(backupPath)) {
        const auto backupResult = atomicallyWrite(backupPath, legacyBytes.value());
        if (!backupResult.hasValue()) {
            return Result<ConfigDocument>::failure(backupResult.error());
        }
    }

    const auto saveResult = save(migrated.value());
    if (!saveResult.hasValue()) {
        return Result<ConfigDocument>::failure(saveResult.error());
    }
    return migrated;
}

Result<void> ConfigRepository::save(const ConfigDocument& document) const
{
    const auto validation = validate(document);
    if (!validation.hasValue()) {
        return validation;
    }
    return atomicallyWrite(configPath_, serialize(document));
}

const QString& ConfigRepository::path() const noexcept
{
    return configPath_;
}

} // namespace Core
