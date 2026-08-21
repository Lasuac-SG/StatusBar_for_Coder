#pragma once
#include <windows.h>
#include <cstdint>

namespace Platform {
    class AppBarProxy {
    public:
        static void Initialize(uint32_t width, uint32_t height) noexcept;
        static void Shutdown() noexcept;

    private:
        static HWND s_proxyHwnd;
        static uint32_t s_width;
        static uint32_t s_height;
        static bool s_isRegistered;
        static UINT s_taskbarRestartMessage;
        static const UINT WM_APPBAR_CALLBACK = WM_USER + 1024;

        static void RegisterAppBar() noexcept;
        static void SetAppBarPos() noexcept;
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };
}
