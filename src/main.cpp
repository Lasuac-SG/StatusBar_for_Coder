#include "src/core/window_manager.h"
#include "src/ui/qt_window_adapter.h"
#include "src/ui/viewmodels/registry_setup.h"
#include "src/platform/display.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    try {
        // 1. 注册所有小组件图纸
        UI::RegisterAllWidgets();

        // 2. 实例化 Qt 前端适配器 (必须传入 argc, argv)
        auto qtAdapter = std::make_unique<UI::QtWindowAdapter>(argc, argv);

        // 3. 依赖注入：将 Qt 适配器交给核心层的 WindowManager 接管
        Core::WindowManager windowManager(std::move(qtAdapter));

        // 4. 获取物理屏幕信息
        int screenWidth = Platform::Display::GetPrimaryScreenWidth();
        uint32_t systemDpi = Platform::Display::GetSystemDpi();

        // 5. 核心层统一调度：驻留屏幕边缘、生成托盘图标、调整缩放
        windowManager.DockToTop(screenWidth, systemDpi);

        // 6. 核心层发号施令，正式启动前端事件循环
        windowManager.Show();

    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
