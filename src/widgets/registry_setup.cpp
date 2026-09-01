#include "widgets/registry_setup.h"
#include "widgets/widget_registry.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"

namespace Widgets {
    void RegisterAllWidgets() {
        WidgetRegistry::GetInstance().Register("Clock", []() -> std::unique_ptr<IWidgetViewModel> {
            return std::make_unique<ClockViewModel>();
        });
        WidgetRegistry::GetInstance().Register("Cpu", []() -> std::unique_ptr<IWidgetViewModel> {
            return std::make_unique<CpuViewModel>();
        });
    }
}
