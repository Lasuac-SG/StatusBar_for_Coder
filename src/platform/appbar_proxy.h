#pragma once
#include "core/result.h"

#include <windows.h>
#include <cstdint>

namespace Platform {
    class AppBarProxy {
    public:
        [[nodiscard]] static Core::Result<void> Initialize(
            HWND rootHwnd,
            uint32_t width,
            uint32_t height);
        static void Shutdown() noexcept;

    private:
        static HWND s_proxyHwnd;
        static HWND s_rootHwnd;
        static uint32_t s_width;
        static uint32_t s_height;
        static bool s_isRegistered;
        static UINT s_taskbarRestartMessage;
        static const UINT WM_APPBAR_CALLBACK = WM_USER + 1024;

        [[nodiscard]] static Core::Result<void> RegisterAppBar();
        [[nodiscard]] static Core::Result<void> SetAppBarPos();
        static LRESULT CALLBACK WndProc(
            HWND hwnd,
            UINT uMsg,
            WPARAM wParam,
            LPARAM lParam) noexcept;
    };
}
