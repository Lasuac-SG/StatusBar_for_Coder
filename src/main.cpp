#include "core/window_manager.h"
#include "ui/qt_window_adapter.h"
#include "widgets/registry_setup.h"
#include "platform/display.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    try {
        Widgets::RegisterAllWidgets();

        auto qtAdapter = std::make_unique<UI::QtWindowAdapter>(argc, argv);
        Core::WindowManager windowManager(std::move(qtAdapter));

        int screenWidth = Platform::Display::GetPrimaryScreenWidth();
        uint32_t systemDpi = Platform::Display::GetSystemDpi();

        windowManager.DockToTop(screenWidth, systemDpi);
        windowManager.Show();

    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
