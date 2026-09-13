#pragma once
#include <windows.h>
#include <functional>

namespace Platform {
    class TrayIcon {
    public:
        TrayIcon();
        ~TrayIcon();

        void Initialize() noexcept;
        void SetQuitCallback(std::function<void()> callback) noexcept;

    private:
        HWND m_messageHwnd = nullptr;
        std::function<void()> m_quitCallback;
        static constexpr UINT WM_TRAY_CALLBACK = WM_USER + 2048;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        void ShowContextMenu() const;
    };
}
