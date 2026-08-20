#include "src/ui/viewmodels/registry_setup.h"
#include "src/ui/viewmodels/widget_registry.h"
#include "src/ui/viewmodels/clock_view_model.h"
// 未来新增组件只需在这里 #include 对应的头文件

namespace UI {
    void RegisterAllWidgets() {
        auto& registry = WidgetRegistry::GetInstance();

        // 注册时钟组件
        registry.Register("Clock", [](const slint::ComponentHandle<MainWindow>& handle) {
            return std::make_unique<ClockViewModel>(handle);
        });

        // 示例：未来如果有 Cpu 组件，直接在此追加
        // registry.Register("Cpu", [](const slint::ComponentHandle<MainWindow>& handle) {
        //     return std::make_unique<CpuViewModel>(handle);
        // });
    }
}
