#include "platform/appbar_proxy.h"
#include "platform/windows_logging.h"

#include <shellapi.h>

#include <exception>

namespace Platform {
    HWND AppBarProxy::s_proxyHwnd = nullptr;
    HWND AppBarProxy::s_rootHwnd = nullptr;
    uint32_t AppBarProxy::s_width = 0;
    uint32_t AppBarProxy::s_height = 0;
    bool AppBarProxy::s_isRegistered = false;
    UINT AppBarProxy::s_taskbarRestartMessage = 0;

    Core::Result<void> AppBarProxy::Initialize(
        HWND rootHwnd,
        uint32_t width,
        uint32_t height) {
        Shutdown();
        if (rootHwnd == nullptr || !IsWindow(rootHwnd)) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot initialize AppBar: root HWND is invalid"));
        }
        if (width == 0 || height == 0) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot initialize AppBar: dimensions must be positive"));
        }

        s_rootHwnd = rootHwnd;
        s_width = width;
        s_height = height;
        s_taskbarRestartMessage = RegisterWindowMessageA("TaskbarCreated");
        if (s_taskbarRestartMessage == 0) {
            s_rootHwnd = nullptr;
            return Core::Result<void>::failure(
                QStringLiteral("Cannot initialize AppBar: TaskbarCreated message registration failed"));
        }

        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "GeekDashboardAppBarProxy";
        const ATOM classAtom = RegisterClassExA(&wc);
        const DWORD classError = classAtom == 0 ? GetLastError() : ERROR_SUCCESS;
        if (classAtom == 0 && classError != ERROR_CLASS_ALREADY_EXISTS) {
            s_rootHwnd = nullptr;
            return Core::Result<void>::failure(
                QStringLiteral("Cannot initialize AppBar proxy class (Win32 error %1)")
                    .arg(classError));
        }

        s_proxyHwnd = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            "GeekDashboardAppBarProxy",
            "GeekAppBarProxyWindow",
            WS_POPUP,
            0, 0, 1, 1, 
            nullptr, nullptr, wc.hInstance, nullptr
        );
        if (s_proxyHwnd == nullptr) {
            const DWORD error = GetLastError();
            s_rootHwnd = nullptr;
            return Core::Result<void>::failure(
                QStringLiteral("Cannot create AppBar proxy window (Win32 error %1)")
                    .arg(error));
        }

        auto result = RegisterAppBar();
        if (!result.hasValue()) {
            Shutdown();
            return result;
        }
        return Core::Result<void>::success();
    }

    Core::Result<void> AppBarProxy::RegisterAppBar() {
        if (s_proxyHwnd == nullptr) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot register AppBar: proxy HWND is missing"));
        }
        APPBARDATA abd{};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = s_proxyHwnd;
        abd.uCallbackMessage = WM_APPBAR_CALLBACK;
        abd.uEdge = ABE_TOP;
        s_isRegistered = SHAppBarMessage(ABM_NEW, &abd);
        if (!s_isRegistered) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot register AppBar with the Windows shell (ABM_NEW failed)"));
        }

        auto result = SetAppBarPos();
        if (!result.hasValue()) {
            APPBARDATA removeData{};
            removeData.cbSize = sizeof(APPBARDATA);
            removeData.hWnd = s_proxyHwnd;
            SHAppBarMessage(ABM_REMOVE, &removeData);
            s_isRegistered = false;
            return result;
        }
        return Core::Result<void>::success();
    }

    Core::Result<void> AppBarProxy::SetAppBarPos() {
        if (s_proxyHwnd == nullptr || s_rootHwnd == nullptr || !s_isRegistered) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot position AppBar: registration is incomplete"));
        }
        APPBARDATA abd{};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = s_proxyHwnd;
        abd.uEdge = ABE_TOP;
        abd.rc = {
            0,
            0,
            static_cast<LONG>(s_width),
            static_cast<LONG>(s_height),
        };
        if (SHAppBarMessage(ABM_QUERYPOS, &abd) == 0) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot position AppBar: ABM_QUERYPOS failed"));
        }
        abd.rc.top = 0; 
        abd.rc.bottom = static_cast<LONG>(s_height);
        if (SHAppBarMessage(ABM_SETPOS, &abd) == 0) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot position AppBar: ABM_SETPOS failed"));
        }
        if (!SetWindowPos(
                s_rootHwnd,
                nullptr,
                abd.rc.left,
                abd.rc.top,
                abd.rc.right - abd.rc.left,
                abd.rc.bottom - abd.rc.top,
                SWP_NOACTIVATE | SWP_NOZORDER)) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot position root AppBar window (Win32 error %1)")
                    .arg(GetLastError()));
        }
        return Core::Result<void>::success();
    }

    void AppBarProxy::Shutdown() noexcept {
        if (s_proxyHwnd) {
            if (s_isRegistered) {
                APPBARDATA abd{};
                abd.cbSize = sizeof(APPBARDATA);
                abd.hWnd = s_proxyHwnd;
                SHAppBarMessage(ABM_REMOVE, &abd);
                s_isRegistered = false;
            }
            DestroyWindow(s_proxyHwnd);
            s_proxyHwnd = nullptr;
        }
        s_rootHwnd = nullptr;
        s_width = 0;
        s_height = 0;
        s_taskbarRestartMessage = 0;
    }

    LRESULT CALLBACK AppBarProxy::WndProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam) noexcept {
        try {
            if (uMsg == WM_APPBAR_CALLBACK) {
                if (wParam == ABN_POSCHANGED) {
                    const auto result = SetAppBarPos();
                    if (!result.hasValue()) {
                        LogWindowsMessage(
                            WindowsLogLevel::Error,
                            QStringLiteral("AppBar position callback failed: %1")
                                .arg(result.error()));
                    }
                    return 0;
                }
            } else if (uMsg == s_taskbarRestartMessage) {
                const auto result = RegisterAppBar();
                if (!result.hasValue()) {
                    LogWindowsMessage(
                        WindowsLogLevel::Error,
                        QStringLiteral("AppBar re-registration failed after shell restart: %1")
                            .arg(result.error()));
                }
                return 0;
            }

            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        } catch (const std::exception&) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Unhandled exception in AppBar window procedure"));
            return 0;
        } catch (...) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Unknown exception in AppBar window procedure"));
            return 0;
        }
    }
}
