#include "src/platform/appbar_proxy.h"
#include <shellapi.h>
#include <iostream>

namespace Platform {
    HWND AppBarProxy::s_proxyHwnd = nullptr;
    uint32_t AppBarProxy::s_width = 0;
    uint32_t AppBarProxy::s_height = 0;
    UINT AppBarProxy::s_taskbarRestartMessage = 0;

    void AppBarProxy::Initialize(uint32_t width, uint32_t height) noexcept {
        s_width = width;
        s_height = height;
        s_taskbarRestartMessage = RegisterWindowMessageA("TaskbarCreated");

        // 1. 注册纯正的 Win32 原生窗口类
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "GeekDashboardAppBarProxy";
        RegisterClassExA(&wc);

        // 2. 创建一个纯粹在后台运行的“幽灵窗口”（宽度和高度无所谓，因为它不可见）
        // WS_POPUP 且不带 WS_VISIBLE，使得它永远不会在屏幕上绘制任何像素
        s_proxyHwnd = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            "GeekDashboardAppBarProxy",
            "GeekAppBarProxyWindow",
            WS_POPUP,
            0, 0, 1, 1, 
            nullptr, nullptr, wc.hInstance, nullptr
        );

        // 3. 派遣代理去执行抢占任务
        Register();
    }

    void AppBarProxy::Register() noexcept {
        APPBARDATA abd = { sizeof(APPBARDATA), s_proxyHwnd, WM_APPBAR_CALLBACK, ABE_TOP, {0, 0, (LONG)s_width, (LONG)s_height} };
        SHAppBarMessage(ABM_NEW, &abd);
        
        // 询问系统建议坐标
        SHAppBarMessage(ABM_QUERYPOS, &abd);
        
        // [核心架构级修复]: 无视系统的任何避让建议（防范僵尸 AppBar 挤占空间）
        // 强行把空间锁死在物理屏幕的最顶端
        abd.rc.top = 0; 
        abd.rc.bottom = s_height;
        
        // 提交最终物理矩阵
        SHAppBarMessage(ABM_SETPOS, &abd);
        std::cout << "[System] 原生代理已成功割让全局工作区。" << std::endl;
    }

    void AppBarProxy::Shutdown() noexcept {
        if (s_proxyHwnd) {
            APPBARDATA abd = { sizeof(APPBARDATA), s_proxyHwnd };
            SHAppBarMessage(ABM_REMOVE, &abd);
            DestroyWindow(s_proxyHwnd);
            s_proxyHwnd = nullptr;
            std::cout << "[System] 原生代理已归还空间并销毁。" << std::endl;
        }
    }

    // 纯净的消息循环，只处理空间校验，没有任何 Subclass 钩子的副作用
    LRESULT CALLBACK AppBarProxy::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_APPBAR_CALLBACK && wParam == ABN_POSCHANGED) {
            Register();
            return 0;
        } else if (uMsg == s_taskbarRestartMessage) {
            Register();
            return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
