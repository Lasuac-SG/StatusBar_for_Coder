#include "widgets/registry_setup.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"

#include <utility>
#include <vector>

namespace Widgets {

Core::Result<WidgetRegistry> registerAllWidgets()
{
    std::vector<WidgetDescriptor> descriptors;
    descriptors.push_back({
        QStringLiteral("Clock"),
        3,
        QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/ClockWidget.qml")),
        [](const Core::WidgetConfig& config, WidgetContext&)
            -> std::unique_ptr<WidgetViewModel> {
            return std::make_unique<ClockViewModel>(config);
        },
    });
    descriptors.push_back({
        QStringLiteral("Cpu"),
        2,
        QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/CpuWidget.qml")),
        [](const Core::WidgetConfig& config, WidgetContext& context)
            -> std::unique_ptr<WidgetViewModel> {
            return std::make_unique<CpuViewModel>(config, context.cpuService);
        },
    });
    return WidgetRegistry::create(std::move(descriptors));
}

} // namespace Widgets
