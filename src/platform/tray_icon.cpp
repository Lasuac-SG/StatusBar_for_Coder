#define WIN32_LEAN_AND_MEAN
#include "src/platform/tray_icon.h"
#include "src/platform/resource.h"
#include <shellapi.h>

namespace Platform {
    TrayIcon::TrayIcon() = default;

    TrayIcon::~TrayIcon() {
        if (m_messageHwnd) {
            NOTIFYICONDATAA nid = { sizeof(NOTIFYICONDATAA) };
            nid.hWnd = m_messageHwnd;
            nid.uID = 1;
            // 回收托盘图标资源
            Shell_NotifyIconA(NIM_DELETE, &nid);
            DestroyWindow(m_messageHwnd);
        }
    }

    void TrayIcon::Initialize() noexcept {
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "GeekDashboardTrayMsgWindow";
        RegisterClassExA(&wc);

        // [核心架构]：创建一个 HWND_MESSAGE 类型的窗口。
        // 此类窗口不会被显示，不会被枚举，专用于后台消息循环收发。
        m_messageHwnd = CreateWindowExA(
            0, "GeekDashboardTrayMsgWindow", nullptr,
            0, 0, 0, 0, 0, 
            HWND_MESSAGE, nullptr, wc.hInstance, this
        );

        NOTIFYICONDATAA nid = { sizeof(NOTIFYICONDATAA) };
        nid.hWnd = m_messageHwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY_CALLBACK;
        
        // 此处加载应用默认图标。后续可通过 LoadImage 加载自定义 .ico 文件
        nid.hIcon = LoadIconA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(IDI_APP_ICON));
        // 悬浮提示文本
        strcpy_s(nid.szTip, "StatusBar"); 

        // 向系统注册托盘图标
        Shell_NotifyIconA(NIM_ADD, &nid);
    }

    void TrayIcon::SetQuitCallback(std::function<void()> callback) noexcept {
        m_quitCallback = std::move(callback);
    }

    void TrayIcon::ShowContextMenu() const {
        POINT pt;
        GetCursorPos(&pt); // 获取当前鼠标物理位置

        HMENU hMenu = CreatePopupMenu();
        // 1001 是菜单项的自定义标识符 (Command ID)
        InsertMenuA(hMenu, 0, MF_BYPOSITION | MF_STRING, 1001, "Quit Dashboard");

        // [Win32 经典 Bug 修复]：
        // 必须在显示菜单前强制将窗口设为前台，否则点击菜单外部时，菜单不会自动消失
        SetForegroundWindow(m_messageHwnd);

        // 阻塞弹出菜单。TPM_RETURNCMD 指示函数直接返回用户点击的标识符
        int cmdId = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_messageHwnd, nullptr);
        DestroyMenu(hMenu);

        // 执行回调
        if (cmdId == 1001 && m_quitCallback) {
            m_quitCallback();
        }
    }

    LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        TrayIcon* pThis = nullptr;

        // [指针注入]：在窗口创建的瞬间，将 C++ 类实例指针与 HWND 绑定，实现面向对象的 WndProc
        if (uMsg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<TrayIcon*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<TrayIcon*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    LRESULT TrayIcon::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_TRAY_CALLBACK) {
            // LOWORD(lParam) 存储了导致托盘图标发送消息的具体鼠标事件
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                ShowContextMenu();
            }
            return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
