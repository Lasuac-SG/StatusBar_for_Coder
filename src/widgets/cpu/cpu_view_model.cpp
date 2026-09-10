#include "widgets/cpu/cpu_view_model.h"

#include <utility>

namespace Widgets {

CpuViewModel::CpuViewModel(
    Core::WidgetConfig config,
    Platform::CpuService& service,
    QObject* parent)
    : WidgetViewModel(parent)
    , m_config(std::move(config))
    , m_service(&service)
    , m_history(service.history())
    , m_cpuPercent(service.cpuPercent())
    , m_currentFrequencyMHz(service.currentFrequencyMHz())
    , m_maxFrequencyMHz(service.maxFrequencyMHz())
    , m_physicalCores(service.physicalCores())
    , m_logicalCores(service.logicalCores())
{
    connect(
        &service,
        &Platform::CpuService::cpuPercentChanged,
        this,
        &CpuViewModel::syncCpuPercent);
    connect(
        &service,
        &Platform::CpuService::currentFrequencyMHzChanged,
        this,
        &CpuViewModel::syncCurrentFrequency);
    connect(
        &service,
        &Platform::CpuService::historyChanged,
        this,
        &CpuViewModel::syncHistory);
    m_consumerLease = service.acquireConsumer();
}

void CpuViewModel::syncCpuPercent()
{
    if (m_service == nullptr || m_cpuPercent == m_service->cpuPercent()) {
        return;
    }
    m_cpuPercent = m_service->cpuPercent();
    emit cpuPercentChanged();
}

void CpuViewModel::syncCurrentFrequency()
{
    if (m_service == nullptr
        || m_currentFrequencyMHz == m_service->currentFrequencyMHz()) {
        return;
    }
    m_currentFrequencyMHz = m_service->currentFrequencyMHz();
    emit currentFrequencyMHzChanged();
}

void CpuViewModel::syncHistory()
{
    if (m_service == nullptr || m_history == m_service->history()) {
        return;
    }
    m_history = m_service->history();
    emit historyChanged();
}

} // namespace Widgets
