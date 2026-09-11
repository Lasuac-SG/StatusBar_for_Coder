#define WIN32_LEAN_AND_MEAN
#include "platform/tray_icon.h"
#include "platform/resource.h"
#include <shellapi.h>

namespace Platform {
    TrayIcon::TrayIcon() = default;

    TrayIcon::~TrayIcon() {
        if (m_messageHwnd) {
            if (m_isRegistered) {
                NOTIFYICONDATAA nid{};
                nid.cbSize = sizeof(NOTIFYICONDATAA);
                nid.hWnd = m_messageHwnd;
                nid.uID = 1;
                Shell_NotifyIconA(NIM_DELETE, &nid);
            }
            DestroyWindow(m_messageHwnd);
        }
    }

    Core::Result<void> TrayIcon::Initialize() {
        if (m_messageHwnd != nullptr) {
            return Core::Result<void>::success();
        }

        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "GeekDashboardTrayMsgWindow";
        const ATOM classAtom = RegisterClassExA(&wc);
        const DWORD classError = classAtom == 0 ? GetLastError() : ERROR_SUCCESS;
        if (classAtom == 0 && classError != ERROR_CLASS_ALREADY_EXISTS) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot register tray window class (Win32 error %1)")
                    .arg(classError));
        }

        m_messageHwnd = CreateWindowExA(
            0, "GeekDashboardTrayMsgWindow", nullptr,
            0, 0, 0, 0, 0, 
            HWND_MESSAGE, nullptr, wc.hInstance, this
        );
        if (m_messageHwnd == nullptr) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot create tray message window (Win32 error %1)")
                    .arg(GetLastError()));
        }

        NOTIFYICONDATAA nid{};
        nid.cbSize = sizeof(NOTIFYICONDATAA);
        nid.hWnd = m_messageHwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY_CALLBACK;
        nid.hIcon = LoadIconA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(IDI_APP_ICON));
        if (nid.hIcon == nullptr) {
            const DWORD error = GetLastError();
            DestroyWindow(m_messageHwnd);
            m_messageHwnd = nullptr;
            return Core::Result<void>::failure(
                QStringLiteral("Cannot load tray icon resource (Win32 error %1)").arg(error));
        }
        strcpy_s(nid.szTip, "StatusBar"); 

        if (!Shell_NotifyIconA(NIM_ADD, &nid)) {
            DestroyWindow(m_messageHwnd);
            m_messageHwnd = nullptr;
            return Core::Result<void>::failure(
                QStringLiteral("Cannot add the StatusBar notification icon"));
        }
        m_isRegistered = true;
        return Core::Result<void>::success();
    }

    void TrayIcon::SetQuitCallback(std::function<void()> callback) noexcept {
        m_quitCallback = std::move(callback);
    }

    void TrayIcon::ShowContextMenu() const {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        InsertMenuA(hMenu, 0, MF_BYPOSITION | MF_STRING, 1001, "Quit Dashboard");

        SetForegroundWindow(m_messageHwnd);
        int cmdId = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_messageHwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmdId == 1001 && m_quitCallback) {
            m_quitCallback();
        }
    }

    LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        TrayIcon* pThis = nullptr;
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
        if (uMsg == WM_TRAY_CALLBACK && LOWORD(lParam) == WM_RBUTTONUP) {
            ShowContextMenu();
            return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
