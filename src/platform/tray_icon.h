#pragma once
#include <windows.h>
#include <functional>

namespace Platform {
    class TrayIcon {
    public:
        TrayIcon();
        ~TrayIcon();

        // 初始化托盘图标并建立系统消息通道
        void Initialize() noexcept;
        
        // 依赖注入：暴露回调函数，让外部决定点击“退出”时执行什么逻辑
        void SetQuitCallback(std::function<void()> callback) noexcept;

    private:
        HWND m_messageHwnd = nullptr;
        std::function<void()> m_quitCallback;
        
        // 托盘自定义消息 ID
        static constexpr UINT WM_TRAY_CALLBACK = WM_USER + 2048;

        // Win32 窗口消息路由钩子
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        
        // 绘制并弹出右键菜单
        void ShowContextMenu() const;
    };
}
