#pragma once

#include "core/result.h"
#include "core/widget_config.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace Core {

class ConfigRepository;

namespace Internal {
class ConfigRepositoryOperations;
[[nodiscard]] ConfigRepository makeConfigRepository(
    QString configPath,
    QStringList legacyCandidates,
    std::shared_ptr<ConfigRepositoryOperations> operations);
}

class ConfigRepository final {
public:
    explicit ConfigRepository(QString configPath, QStringList legacyCandidates = {});

    [[nodiscard]] Result<ConfigDocument> load() const;
    [[nodiscard]] Result<void> save(const ConfigDocument& document) const;
    [[nodiscard]] const QString& path() const noexcept;

private:
    ConfigRepository(
        QString configPath,
        QStringList legacyCandidates,
        std::shared_ptr<Internal::ConfigRepositoryOperations> operations);

    friend ConfigRepository Internal::makeConfigRepository(
        QString configPath,
        QStringList legacyCandidates,
        std::shared_ptr<Internal::ConfigRepositoryOperations> operations);

    QString configPath_;
    QStringList legacyCandidates_;
    std::shared_ptr<Internal::ConfigRepositoryOperations> operations_;
};

} // namespace Core
