#include "src/core/window_manager.h"
#include "src/platform/display.h"
#include <windows.h>
#include <iostream>
#include <cstdio>
#include <stdlib.h> // 需要用到 _putenv

int main() {
    // 强制声明本进程处理所有的物理 DPI，防止 Windows 对我们进行拉伸模糊处理
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // [核心修复]：强制关闭 Slint 内部的自动 DPI 二次缩放行为！
    // 既然我们在 C++ 层提供了绝对物理像素，就绝不允许 UI 引擎内部再乘一遍倍率。
    _putenv("SLINT_SCALE_FACTOR=1"); 

    Core::WindowManager windowManager;

    int screenWidth = Platform::Display::GetPrimaryScreenWidth();
    uint32_t dpi = Platform::Display::GetSystemDpi();

    std::cout << "[Info] 物理屏幕宽度: " << screenWidth << " px" << std::endl;
    std::cout << "[Info] 当前系统 DPI: " << dpi << std::endl;

    windowManager.DockToTop(screenWidth, dpi);
    windowManager.Show();

    return 0;
}
