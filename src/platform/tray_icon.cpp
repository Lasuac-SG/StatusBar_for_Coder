#define WIN32_LEAN_AND_MEAN
#include "platform/tray_icon.h"

#include "platform/resource.h"
#include "platform/windows_logging.h"

#include <shellapi.h>

#include <algorithm>
#include <exception>
#include <iterator>
#include <utility>

namespace Platform {
namespace {

constexpr UINT quitCommand = 1001;
constexpr wchar_t windowClassName[] = L"StatusBarForCoder.TrayMessageWindow";

bool setInstance(const HWND window, TrayIcon* const instance, DWORD& error) noexcept
{
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(
        window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
    error = GetLastError();
    return previous != 0 || error == ERROR_SUCCESS;
}

class Menu final {
public:
    explicit Menu(const HMENU handle) noexcept
        : handle_(handle)
    {
    }

    ~Menu()
    {
        if (release() != ERROR_SUCCESS) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot destroy tray popup menu during cleanup"));
        }
    }

    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu(Menu&&) = delete;
    Menu& operator=(Menu&&) = delete;

    [[nodiscard]] HMENU get() const noexcept { return handle_; }

    [[nodiscard]] DWORD release() noexcept
    {
        if (handle_ == nullptr) {
            return ERROR_SUCCESS;
        }
        const HMENU handle = std::exchange(handle_, nullptr);
        SetLastError(ERROR_SUCCESS);
        if (DestroyMenu(handle)) {
            return ERROR_SUCCESS;
        }
        const DWORD error = GetLastError();
        return error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
    }

private:
    HMENU handle_{};
};

} // namespace

TrayIcon::~TrayIcon()
{
    shutdown();
}

Core::Result<void> TrayIcon::initialize(std::function<void()> quitCallback)
{
    if (messageWindow_ != nullptr) {
        return Core::Result<void>::success();
    }

    SetLastError(ERROR_SUCCESS);
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (taskbarCreatedMessage_ == 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot register TaskbarCreated for tray recovery (Win32 error %1)")
                .arg(GetLastError()));
    }
    quitCallback_ = std::move(quitCallback);

    SetLastError(ERROR_SUCCESS);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (instance == nullptr) {
        const DWORD error = GetLastError();
        shutdown();
        return Core::Result<void>::failure(
            QStringLiteral("Cannot obtain the tray module handle (Win32 error %1)")
                .arg(error));
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = windowClassName;
    SetLastError(ERROR_SUCCESS);
    const ATOM atom = RegisterClassExW(&windowClass);
    const DWORD classError = atom == 0 ? GetLastError() : ERROR_SUCCESS;
    if (atom == 0 && classError != ERROR_CLASS_ALREADY_EXISTS) {
        shutdown();
        return Core::Result<void>::failure(
            QStringLiteral("Cannot register tray window class (Win32 error %1)")
                .arg(classError));
    }

    SetLastError(ERROR_SUCCESS);
    messageWindow_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        windowClassName,
        L"",
        WS_POPUP,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        this);
    if (messageWindow_ == nullptr) {
        const DWORD error = GetLastError();
        shutdown();
        return Core::Result<void>::failure(
            QStringLiteral("Cannot create or bind tray message window (Win32 error %1)")
                .arg(error));
    }

    const auto result = addNotification();
    if (!result.hasValue()) {
        shutdown();
    }
    return result;
}

Core::Result<void> TrayIcon::addNotification()
{
    if (messageWindow_ == nullptr || !IsWindow(messageWindow_)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot add tray icon: message window is invalid"));
    }

    SetLastError(ERROR_SUCCESS);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (instance == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot obtain the tray module handle (Win32 error %1)")
                .arg(GetLastError()));
    }

    SetLastError(ERROR_SUCCESS);
    const HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (icon == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot load tray icon resource (Win32 error %1)")
                .arg(GetLastError()));
    }

    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = messageWindow_;
    notification.uID = iconId_;
    notification.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notification.uCallbackMessage = callbackMessage_;
    notification.hIcon = icon;
    constexpr wchar_t tooltip[] = L"StatusBar";
    static_assert(std::size(tooltip) <= std::size(notification.szTip));
    std::copy(std::begin(tooltip), std::end(tooltip), std::begin(notification.szTip));

    if (!Shell_NotifyIconW(NIM_ADD, &notification)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot add the StatusBar notification icon"));
    }
    registered_ = true;

    notification.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &notification)) {
        NOTIFYICONDATAW removal{};
        removal.cbSize = sizeof(removal);
        removal.hWnd = messageWindow_;
        removal.uID = iconId_;
        if (!Shell_NotifyIconW(NIM_DELETE, &removal)) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot roll back tray icon after NIM_SETVERSION failure"));
        }
        registered_ = false;
        return Core::Result<void>::failure(
            QStringLiteral("Cannot select the tray notification protocol version"));
    }

    return Core::Result<void>::success();
}

Core::Result<void> TrayIcon::recoverAfterShellRestart()
{
    if (messageWindow_ == nullptr || !IsWindow(messageWindow_)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot recover tray icon: message window is invalid"));
    }
    registered_ = false;
    return addNotification();
}

Core::Result<void> TrayIcon::showContextMenu()
{
    POINT point{};
    SetLastError(ERROR_SUCCESS);
    if (!GetCursorPos(&point)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot locate cursor for tray menu (Win32 error %1)")
                .arg(GetLastError()));
    }

    SetLastError(ERROR_SUCCESS);
    Menu menu(CreatePopupMenu());
    if (menu.get() == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot create tray popup menu (Win32 error %1)")
                .arg(GetLastError()));
    }
    SetLastError(ERROR_SUCCESS);
    if (!InsertMenuW(
            menu.get(), 0, MF_BYPOSITION | MF_STRING, quitCommand, L"Quit StatusBar")) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot populate tray popup menu (Win32 error %1)")
                .arg(GetLastError()));
    }
    if (!SetForegroundWindow(messageWindow_)) {
        LogWindowsMessage(
            WindowsLogLevel::Warning,
            QStringLiteral("Windows denied foreground activation for the tray popup menu"));
    }

    SetLastError(ERROR_SUCCESS);
    const UINT command = TrackPopupMenu(
        menu.get(),
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        messageWindow_,
        nullptr);
    const DWORD trackingError = GetLastError();

    const DWORD closeError = menu.release();

    SetLastError(ERROR_SUCCESS);
    const bool posted = PostMessageW(messageWindow_, WM_NULL, 0, 0);
    const DWORD postError = GetLastError();

    if (command == quitCommand && quitCallback_) {
        quitCallback_();
    }
    if (command == 0 && trackingError != ERROR_SUCCESS) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot track tray popup menu (Win32 error %1)")
                .arg(trackingError));
    }
    if (closeError != ERROR_SUCCESS) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot destroy tray popup menu (Win32 error %1)")
                .arg(closeError));
    }
    if (!posted) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot finalize tray popup menu (Win32 error %1)")
                .arg(postError));
    }
    return Core::Result<void>::success();
}

void TrayIcon::destroyMessageWindow() noexcept
{
    try {
        if (messageWindow_ == nullptr) {
            return;
        }

        const HWND window = std::exchange(messageWindow_, nullptr);
        DWORD bindingError = ERROR_SUCCESS;
        const bool bindingCleared = setInstance(window, nullptr, bindingError);

        SetLastError(ERROR_SUCCESS);
        const bool destroyed = DestroyWindow(window);
        const DWORD destroyError = GetLastError();

        if (!bindingCleared) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot clear tray instance binding (Win32 error %1)")
                    .arg(bindingError));
        }
        if (!destroyed) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot destroy tray message window (Win32 error %1)")
                    .arg(destroyError));
        }
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception while destroying the tray message window"));
    }
}

void TrayIcon::shutdown() noexcept
{
    try {
        if (registered_ && messageWindow_ != nullptr) {
            NOTIFYICONDATAW notification{};
            notification.cbSize = sizeof(notification);
            notification.hWnd = messageWindow_;
            notification.uID = iconId_;
            if (!Shell_NotifyIconW(NIM_DELETE, &notification)) {
                LogWindowsMessage(
                    WindowsLogLevel::Error,
                    QStringLiteral("Cannot remove the StatusBar notification icon"));
            }
        }
        registered_ = false;
        destroyMessageWindow();
        quitCallback_ = {};
        taskbarCreatedMessage_ = 0;
    } catch (const std::exception&) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unhandled exception while shutting down tray icon"));
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception while shutting down tray icon"));
    }
}

LRESULT CALLBACK TrayIcon::windowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) noexcept
{
    try {
        TrayIcon* instance{};
        if (message == WM_NCCREATE) {
            const auto* const create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            if (create == nullptr || create->lpCreateParams == nullptr) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            instance = static_cast<TrayIcon*>(create->lpCreateParams);
            DWORD error = ERROR_SUCCESS;
            if (!setInstance(window, instance, error)) {
                SetLastError(error);
                return FALSE;
            }
        } else {
            instance = reinterpret_cast<TrayIcon*>(
                GetWindowLongPtrW(window, GWLP_USERDATA));
        }

        if (message == WM_NCDESTROY) {
            if (instance != nullptr && instance->messageWindow_ == window) {
                instance->messageWindow_ = nullptr;
                instance->registered_ = false;
            }
            DWORD error = ERROR_SUCCESS;
            if (!setInstance(window, nullptr, error)) {
                LogWindowsMessage(
                    WindowsLogLevel::Error,
                    QStringLiteral("Cannot clear tray instance during teardown (Win32 error %1)")
                        .arg(error));
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }
        if (instance != nullptr) {
            return instance->handleMessage(window, message, wParam, lParam);
        }
        return DefWindowProcW(window, message, wParam, lParam);
    } catch (const std::exception&) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unhandled exception in tray window procedure"));
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception in tray window procedure"));
    }

    if (message == WM_NCCREATE) {
        SetLastError(ERROR_UNHANDLED_EXCEPTION);
        return FALSE;
    }
    return 0;
}

LRESULT TrayIcon::handleMessage(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam)
{
    const UINT notification = LOWORD(lParam);
    if (message == callbackMessage_
        && (notification == WM_RBUTTONUP || notification == WM_CONTEXTMENU)) {
        const auto result = showContextMenu();
        if (!result.hasValue()) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Tray menu failed: %1").arg(result.error()));
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace Platform
