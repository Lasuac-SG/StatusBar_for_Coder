#include "widgets/cpu/cpu_view_model.h"

#include <utility>

namespace Widgets {

CpuViewModel::CpuViewModel(
    Core::WidgetConfig config,
    Platform::CpuService& service,
    QObject* parent)
    : IWidgetViewModel(parent)
    , m_config(std::move(config))
    , m_service(service)
{
    connect(
        &m_service,
        &Platform::CpuService::cpuPercentChanged,
        this,
        &CpuViewModel::cpuPercentChanged);
    connect(
        &m_service,
        &Platform::CpuService::currentFrequencyMHzChanged,
        this,
        &CpuViewModel::currentFrequencyMHzChanged);
    connect(
        &m_service,
        &Platform::CpuService::historyChanged,
        this,
        &CpuViewModel::historyChanged);
}

} // namespace Widgets
