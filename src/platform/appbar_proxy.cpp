#include "src/platform/appbar_proxy.h"
#include <shellapi.h>
#include <iostream>

namespace Platform {
    HWND AppBarProxy::s_proxyHwnd = nullptr;
    uint32_t AppBarProxy::s_width = 0;
    uint32_t AppBarProxy::s_height = 0;
    bool AppBarProxy::s_isRegistered = false;
    UINT AppBarProxy::s_taskbarRestartMessage = 0;

    void AppBarProxy::Initialize(uint32_t width, uint32_t height) noexcept {
        s_width = width;
        s_height = height;
        s_taskbarRestartMessage = RegisterWindowMessageA("TaskbarCreated");

        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "GeekDashboardAppBarProxy";
        RegisterClassExA(&wc);

        s_proxyHwnd = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            "GeekDashboardAppBarProxy",
            "GeekAppBarProxyWindow",
            WS_POPUP,
            0, 0, 1, 1, 
            nullptr, nullptr, wc.hInstance, nullptr
        );

        RegisterAppBar();
    }

    void AppBarProxy::RegisterAppBar() noexcept {
        if (!s_proxyHwnd) return;

        APPBARDATA abd = { sizeof(APPBARDATA), s_proxyHwnd, WM_APPBAR_CALLBACK, ABE_TOP };
        s_isRegistered = SHAppBarMessage(ABM_NEW, &abd);
        
        SetAppBarPos();
    }

    void AppBarProxy::SetAppBarPos() noexcept {
        if (!s_proxyHwnd || !s_isRegistered) return;

        APPBARDATA abd = { sizeof(APPBARDATA), s_proxyHwnd, 0, ABE_TOP, {0, 0, (LONG)s_width, (LONG)s_height} };
        SHAppBarMessage(ABM_QUERYPOS, &abd);
        
        abd.rc.top = 0; 
        abd.rc.bottom = s_height;
        SHAppBarMessage(ABM_SETPOS, &abd);
    }

    void AppBarProxy::Shutdown() noexcept {
        if (s_proxyHwnd) {
            if (s_isRegistered) {
                APPBARDATA abd = { sizeof(APPBARDATA), s_proxyHwnd };
                SHAppBarMessage(ABM_REMOVE, &abd);
                s_isRegistered = false;
            }
            DestroyWindow(s_proxyHwnd);
            s_proxyHwnd = nullptr;
        }
    }

    LRESULT CALLBACK AppBarProxy::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_APPBAR_CALLBACK) {
            if (wParam == ABN_POSCHANGED) {
                // 仅更新坐标，绝不重复调用 ABM_NEW
                SetAppBarPos();
                return 0;
            }
        } else if (uMsg == s_taskbarRestartMessage) {
            // Explorer 崩溃重启时重新注册
            RegisterAppBar();
            return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
