#pragma once

#include "core/widget_config.h"
#include "platform/cpu_service.h"
#include "widgets/i_widget_view_model.h"

#include <QJsonObject>
#include <QString>
#include <QVariantList>

#include <string>

namespace Widgets {

class CpuViewModel final : public IWidgetViewModel {
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

    [[nodiscard]] const QString& instanceId() const noexcept { return m_config.id; }
    [[nodiscard]] const QJsonObject& settings() const noexcept { return m_config.settings; }
    [[nodiscard]] int cpuPercent() const noexcept { return m_service.cpuPercent(); }
    [[nodiscard]] int currentFrequencyMHz() const noexcept
    {
        return m_service.currentFrequencyMHz();
    }
    [[nodiscard]] int maxFrequencyMHz() const noexcept { return m_service.maxFrequencyMHz(); }
    [[nodiscard]] int physicalCores() const noexcept { return m_service.physicalCores(); }
    [[nodiscard]] int logicalCores() const noexcept { return m_service.logicalCores(); }
    [[nodiscard]] const QVariantList& history() const noexcept { return m_service.history(); }

    [[nodiscard]] int GetSpan() const override { return 2; }
    [[nodiscard]] std::string GetKind() const override { return "Cpu"; }
    void Update() override { }

signals:
    void cpuPercentChanged();
    void currentFrequencyMHzChanged();
    void historyChanged();

private:
    Core::WidgetConfig m_config;
    Platform::CpuService& m_service;
};

} // namespace Widgets
