#pragma once

#include "core/config_repository.h"
#include "core/result.h"

#include <QByteArray>
#include <QLockFile>
#include <QString>
#include <QStringList>

#include <memory>

namespace Core::Internal {

struct LockAttemptResult final {
    bool acquired{};
    QLockFile::LockError error{QLockFile::NoError};
};

class ConfigRepositoryOperations {
public:
    virtual ~ConfigRepositoryOperations() = default;

    [[nodiscard]] virtual LockAttemptResult attemptMigrationLock(
        QLockFile& lock,
        int timeoutMs) = 0;
    [[nodiscard]] virtual Result<void> atomicWrite(
        const QString& path,
        const QByteArray& bytes) = 0;
};

[[nodiscard]] std::shared_ptr<ConfigRepositoryOperations>
defaultConfigRepositoryOperations();

[[nodiscard]] ConfigRepository makeConfigRepository(
    QString configPath,
    QStringList legacyCandidates,
    std::shared_ptr<ConfigRepositoryOperations> operations);

} // namespace Core::Internal
