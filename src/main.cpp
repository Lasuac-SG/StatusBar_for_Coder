#define WIN32_LEAN_AND_MEAN 
#define NOMINMAX 

#include "src/core/window_manager.h"
#include "ui/slint_window_adapter.h"
#include "src/platform/display.h"
#include <windows.h>
#include <iostream>
#include <cstdlib> // 替换旧的 <stdlib.h>

#ifndef NDEBUG
void SetupDebugConsole() {
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    SetConsoleOutputCP(CP_UTF8); 
    std::cout << "[System] 调试控制台已挂载 (Debug Build)。" << std::endl;
}
#endif

int main() {
#ifndef NDEBUG
    SetupDebugConsole(); // 发布时这段代码会自动消失，无冗余开销
#endif

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    _putenv("SLINT_SCALE_FACTOR=1"); 

    auto uiAdapter = std::make_unique<UI::SlintWindowAdapter>();
    Core::WindowManager windowManager(std::move(uiAdapter));

    int screenWidth = Platform::Display::GetPrimaryScreenWidth();
    uint32_t dpi = Platform::Display::GetSystemDpi();

#ifndef NDEBUG
    std::cout << "[Info] 物理屏幕宽度: " << screenWidth << " px" << std::endl;
    std::cout << "[Info] 当前系统 DPI: " << dpi << std::endl;
#endif

    windowManager.DockToTop(screenWidth, dpi);
    windowManager.Show();

    return 0;
}
