#include "src/core/window_manager.h"
#include "ui/slint_window_adapter.h" // 引入具体的 UI 适配器
#include "src/platform/display.h"
#include <windows.h>
#include <iostream>
#include <stdlib.h> 

void SetupDebugConsole() {
    AllocConsole(); // 向系统申请分配一个控制台窗口
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout); // 重定向 cout
    freopen_s(&fp, "CONOUT$", "w", stderr); // 重定向 cerr
    freopen_s(&fp, "CONIN$", "r", stdin);   // 重定向 cin
    
    // 强制控制台支持 UTF-8 显示，防止中文乱码
    SetConsoleOutputCP(CP_UTF8); 
    std::cout << "[System] 调试控制台已挂载。" << std::endl;
}

int main() {
    SetupDebugConsole();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    _putenv("SLINT_SCALE_FACTOR=1"); 

    // 1. 实例化具体的 UI 前端（Slint 适配器）
    // std::make_unique 高效且安全地分配内存
    auto uiAdapter = std::make_unique<UI::SlintWindowAdapter>();

    // 2. 将具体的 UI 前端通过 std::move 所有权转移注入到核心逻辑层中
    Core::WindowManager windowManager(std::move(uiAdapter));

    int screenWidth = Platform::Display::GetPrimaryScreenWidth();
    uint32_t dpi = Platform::Display::GetSystemDpi();

    std::cout << "[Info] 物理屏幕宽度: " << screenWidth << " px" << std::endl;
    std::cout << "[Info] 当前系统 DPI: " << dpi << std::endl;

    // 核心层内部不需要关心 UI 具体是什么技术栈，它只负责运算坐标并调用接口
    windowManager.DockToTop(screenWidth, dpi);
    windowManager.Show();

    return 0;
}
