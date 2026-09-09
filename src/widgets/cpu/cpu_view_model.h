#pragma once

#include "core/widget_config.h"
#include "platform/cpu_service.h"
#include "widgets/widget_view_model.h"

#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

namespace Widgets {

class CpuViewModel final : public WidgetViewModel {
    Q_OBJECT
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QJsonObject settings READ settings CONSTANT)
    Q_PROPERTY(int cpuPercent READ cpuPercent NOTIFY cpuPercentChanged)
    Q_PROPERTY(int currentFrequencyMHz READ currentFrequencyMHz NOTIFY currentFrequencyMHzChanged)
    Q_PROPERTY(int maxFrequencyMHz READ maxFrequencyMHz CONSTANT)
    Q_PROPERTY(int physicalCores READ physicalCores CONSTANT)
    Q_PROPERTY(int logicalCores READ logicalCores CONSTANT)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    CpuViewModel(
        Core::WidgetConfig config,
        Platform::CpuService& service,
        QObject* parent = nullptr);

    [[nodiscard]] const QString& instanceId() const noexcept override { return m_config.id; }
    [[nodiscard]] const QJsonObject& settings() const noexcept { return m_config.settings; }
    [[nodiscard]] int cpuPercent() const noexcept { return m_cpuPercent; }
    [[nodiscard]] int currentFrequencyMHz() const noexcept { return m_currentFrequencyMHz; }
    [[nodiscard]] int maxFrequencyMHz() const noexcept { return m_maxFrequencyMHz; }
    [[nodiscard]] int physicalCores() const noexcept { return m_physicalCores; }
    [[nodiscard]] int logicalCores() const noexcept { return m_logicalCores; }
    [[nodiscard]] const QVariantList& history() const noexcept { return m_history; }

signals:
    void cpuPercentChanged();
    void currentFrequencyMHzChanged();
    void historyChanged();

private:
    void syncCpuPercent();
    void syncCurrentFrequency();
    void syncHistory();

    Core::WidgetConfig m_config;
    QPointer<Platform::CpuService> m_service;
    QVariantList m_history;
    int m_cpuPercent{};
    int m_currentFrequencyMHz{};
    int m_maxFrequencyMHz{};
    int m_physicalCores{};
    int m_logicalCores{};
};

} // namespace Widgets
