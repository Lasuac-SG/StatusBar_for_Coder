#pragma once

#include "core/result.h"
#include "core/widget_config.h"

#include <QString>
#include <QStringList>

namespace Core {

class ConfigRepository final {
public:
    explicit ConfigRepository(QString configPath, QStringList legacyCandidates = {});

    [[nodiscard]] Result<ConfigDocument> load() const;
    [[nodiscard]] Result<void> save(const ConfigDocument& document) const;
    [[nodiscard]] const QString& path() const noexcept;

private:
    QString configPath_;
    QStringList legacyCandidates_;
};

} // namespace Core
