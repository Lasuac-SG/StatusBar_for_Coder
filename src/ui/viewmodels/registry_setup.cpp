#include "src/ui/viewmodels/registry_setup.h"
#include "src/ui/viewmodels/widget_registry.h"
#include "src/ui/viewmodels/clock_view_model.h"

namespace UI {
    void RegisterAllWidgets() {
        auto& registry = WidgetRegistry::GetInstance();

        // 注册时钟组件，不再传递任何 handle
        registry.Register("Clock", []() {
            return std::make_unique<ClockViewModel>();
        });
    }
}
