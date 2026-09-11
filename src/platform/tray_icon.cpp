#define WIN32_LEAN_AND_MEAN
#include "platform/tray_icon.h"

#include "platform/resource.h"
#include "platform/windows_logging.h"

#include <shellapi.h>

#include <exception>
#include <utility>

namespace Platform {

namespace {

constexpr UINT quitMenuCommand = 1001;

bool setWindowInstance(
    const HWND window,
    TrayIcon* const instance,
    DWORD& error) noexcept
{
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtr(
        window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
    error = GetLastError();
    return previous != 0 || error == ERROR_SUCCESS;
}

class MenuHandle final {
public:
    explicit MenuHandle(HMENU menu) noexcept
        : menu_(menu)
    {
    }

    ~MenuHandle()
    {
        try {
            const auto result = close();
            if (!result.hasValue()) {
                LogWindowsMessage(WindowsLogLevel::Error, result.error());
            }
        } catch (...) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Unknown exception while destroying tray popup menu"));
        }
    }

    MenuHandle(const MenuHandle&) = delete;
    MenuHandle& operator=(const MenuHandle&) = delete;
    MenuHandle(MenuHandle&&) = delete;
    MenuHandle& operator=(MenuHandle&&) = delete;

    [[nodiscard]] HMENU get() const noexcept { return menu_; }

    [[nodiscard]] Core::Result<void> close()
    {
        if (menu_ == nullptr) {
            return Core::Result<void>::success();
        }

        SetLastError(ERROR_SUCCESS);
        if (!DestroyMenu(menu_)) {
            return Core::Result<void>::failure(
                QStringLiteral("Cannot destroy tray popup menu (Win32 error %1)")
                    .arg(GetLastError()));
        }
        menu_ = nullptr;
        return Core::Result<void>::success();
    }

private:
    HMENU menu_;
};

} // namespace

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon()
{
    try {
        if (m_messageHwnd == nullptr) {
            return;
        }

        if (m_isRegistered) {
            NOTIFYICONDATAA notification{};
            notification.cbSize = sizeof(NOTIFYICONDATAA);
            notification.hWnd = m_messageHwnd;
            notification.uID = 1;
            if (!Shell_NotifyIconA(NIM_DELETE, &notification)) {
                LogWindowsMessage(
                    WindowsLogLevel::Error,
                    QStringLiteral("Cannot remove the StatusBar notification icon"));
            }
            m_isRegistered = false;
        }

        DestroyMessageWindow();
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception while destroying tray icon"));
    }
}

Core::Result<void> TrayIcon::Initialize()
{
    if (m_messageHwnd != nullptr) {
        return Core::Result<void>::success();
    }

    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXA);
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = GetModuleHandleA(nullptr);
    windowClass.lpszClassName = "GeekDashboardTrayMsgWindow";
    const ATOM classAtom = RegisterClassExA(&windowClass);
    const DWORD classError = classAtom == 0 ? GetLastError() : ERROR_SUCCESS;
    if (classAtom == 0 && classError != ERROR_CLASS_ALREADY_EXISTS) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot register tray window class (Win32 error %1)")
                .arg(classError));
    }

    m_messageHwnd = CreateWindowExA(
        0,
        "GeekDashboardTrayMsgWindow",
        nullptr,
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        windowClass.hInstance,
        this);
    if (m_messageHwnd == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral(
                "Cannot create or bind tray message window (Win32 error %1)")
                .arg(GetLastError()));
    }

    NOTIFYICONDATAA notification{};
    notification.cbSize = sizeof(NOTIFYICONDATAA);
    notification.hWnd = m_messageHwnd;
    notification.uID = 1;
    notification.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notification.uCallbackMessage = WM_TRAY_CALLBACK;
    notification.hIcon =
        LoadIconA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(IDI_APP_ICON));
    if (notification.hIcon == nullptr) {
        const DWORD error = GetLastError();
        DestroyMessageWindow();
        return Core::Result<void>::failure(
            QStringLiteral("Cannot load tray icon resource (Win32 error %1)")
                .arg(error));
    }
    const errno_t tipResult = strcpy_s(notification.szTip, "StatusBar");
    if (tipResult != 0) {
        DestroyMessageWindow();
        return Core::Result<void>::failure(
            QStringLiteral("Cannot prepare the tray icon tooltip (error %1)")
                .arg(tipResult));
    }

    if (!Shell_NotifyIconA(NIM_ADD, &notification)) {
        DestroyMessageWindow();
        return Core::Result<void>::failure(
            QStringLiteral("Cannot add the StatusBar notification icon"));
    }
    m_isRegistered = true;
    return Core::Result<void>::success();
}

void TrayIcon::SetQuitCallback(std::function<void()> callback) noexcept
{
    m_quitCallback = std::move(callback);
}

Core::Result<void> TrayIcon::ShowContextMenu() const
{
    POINT point{};
    SetLastError(ERROR_SUCCESS);
    if (!GetCursorPos(&point)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot locate cursor for tray menu (Win32 error %1)")
                .arg(GetLastError()));
    }

    SetLastError(ERROR_SUCCESS);
    MenuHandle menu(CreatePopupMenu());
    if (menu.get() == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot create tray popup menu (Win32 error %1)")
                .arg(GetLastError()));
    }
    SetLastError(ERROR_SUCCESS);
    if (!InsertMenuA(
            menu.get(),
            0,
            MF_BYPOSITION | MF_STRING,
            quitMenuCommand,
            "Quit Dashboard")) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot populate tray popup menu (Win32 error %1)")
                .arg(GetLastError()));
    }
    if (!SetForegroundWindow(m_messageHwnd)) {
        LogWindowsMessage(
            WindowsLogLevel::Warning,
            QStringLiteral(
                "Windows denied foreground activation for the tray popup menu; continuing"));
    }

    SetLastError(ERROR_SUCCESS);
    const int command = TrackPopupMenu(
        menu.get(),
        TPM_RETURNCMD | TPM_NONOTIFY,
        point.x,
        point.y,
        0,
        m_messageHwnd,
        nullptr);
    const DWORD trackingError = GetLastError();

    SetLastError(ERROR_SUCCESS);
    const bool postedDismissal = PostMessageW(m_messageHwnd, WM_NULL, 0, 0);
    const DWORD postError = GetLastError();

    const auto closeResult = menu.close();
    if (!closeResult.hasValue()) {
        LogWindowsMessage(WindowsLogLevel::Error, closeResult.error());
    }
    if (!postedDismissal) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Cannot finalize tray popup menu (Win32 error %1)")
                .arg(postError));
    }

    if (command == static_cast<int>(quitMenuCommand) && m_quitCallback) {
        m_quitCallback();
    }
    if (command == 0 && trackingError != ERROR_SUCCESS) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot track tray popup menu (Win32 error %1)")
                .arg(trackingError));
    }
    return Core::Result<void>::success();
}

void TrayIcon::DestroyMessageWindow() noexcept
{
    try {
        if (m_messageHwnd == nullptr) {
            return;
        }

        const HWND window = std::exchange(m_messageHwnd, nullptr);
        DWORD bindingError = ERROR_SUCCESS;
        if (!setWindowInstance(window, nullptr, bindingError)) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral(
                    "Cannot clear tray window instance binding (Win32 error %1)")
                    .arg(bindingError));
        }
        if (!DestroyWindow(window)) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot destroy tray message window (Win32 error %1)")
                    .arg(GetLastError()));
        }
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception while destroying tray message window"));
    }
}

LRESULT CALLBACK TrayIcon::WndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam) noexcept
{
    try {
        TrayIcon* instance = nullptr;
        if (uMsg == WM_NCCREATE) {
            const auto* const create = reinterpret_cast<const CREATESTRUCTA*>(lParam);
            if (create == nullptr || create->lpCreateParams == nullptr) {
                SetLastError(ERROR_INVALID_PARAMETER);
                LogWindowsMessage(
                    WindowsLogLevel::Error,
                    QStringLiteral("Tray window creation did not provide an instance binding"));
                return FALSE;
            }

            instance = static_cast<TrayIcon*>(create->lpCreateParams);
            DWORD bindingError = ERROR_SUCCESS;
            if (!setWindowInstance(hwnd, instance, bindingError)) {
                LogWindowsMessage(
                    WindowsLogLevel::Error,
                    QStringLiteral(
                        "Cannot bind tray window instance (Win32 error %1)")
                        .arg(bindingError));
                SetLastError(bindingError);
                return FALSE;
            }
        } else {
            instance = reinterpret_cast<TrayIcon*>(
                GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (uMsg == WM_NCDESTROY) {
            if (instance != nullptr && instance->m_messageHwnd == hwnd) {
                instance->m_messageHwnd = nullptr;
                instance->m_isRegistered = false;
            }
            DWORD bindingError = ERROR_SUCCESS;
            if (!setWindowInstance(hwnd, nullptr, bindingError)) {
                LogWindowsMessage(
                    WindowsLogLevel::Error,
                    QStringLiteral(
                        "Cannot clear tray window instance during teardown "
                        "(Win32 error %1)")
                        .arg(bindingError));
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        if (instance != nullptr) {
            return instance->HandleMessage(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    } catch (const std::exception&) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unhandled exception in tray window procedure"));
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception in tray window procedure"));
    }

    if (uMsg == WM_NCCREATE) {
        SetLastError(ERROR_UNHANDLED_EXCEPTION);
        return FALSE;
    }
    return 0;
}

LRESULT TrayIcon::HandleMessage(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    if (uMsg == WM_TRAY_CALLBACK && LOWORD(lParam) == WM_RBUTTONUP) {
        const auto result = ShowContextMenu();
        if (!result.hasValue()) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Tray menu failed: %1").arg(result.error()));
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace Platform
