#define WIN32_LEAN_AND_MEAN
#include "platform/tray_icon.h"

#include "platform/resource.h"
#include "platform/windows_logging.h"

#include <windowsx.h>

#include <algorithm>
#include <exception>
#include <iterator>
#include <limits>
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

NOTIFYICONDATAW notificationData(const HWND window, const UINT iconId) noexcept
{
    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = window;
    notification.uID = iconId;
    return notification;
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

std::optional<Detail::TrayNotification> Detail::decodeTrayNotification(
    const WPARAM packedPoint,
    const LPARAM packedNotification,
    const UINT iconId) noexcept
{
    if (iconId > std::numeric_limits<WORD>::max()
        || HIWORD(packedNotification) != iconId) {
        return std::nullopt;
    }

    const UINT notification = LOWORD(packedNotification);
    if (notification != WM_RBUTTONUP
        && notification != WM_CONTEXTMENU
        && notification != NIN_KEYSELECT) {
        return std::nullopt;
    }

    POINT point{};
    if (notification == WM_RBUTTONUP) {
        point = {GET_X_LPARAM(packedPoint), GET_Y_LPARAM(packedPoint)};
    }
    return Detail::TrayNotification{
        notification,
        point,
        notification != WM_RBUTTONUP,
    };
}

TrayIcon::~TrayIcon()
{
    shutdown();
}

Core::Result<void> TrayIcon::initialize(
    const UINT taskbarCreatedMessage,
    std::function<void()> quitCallback)
{
    if (api_.notifyIcon == nullptr
        || api_.notifyIconRect == nullptr
        || api_.loadIcon == nullptr
        || api_.trackPopupMenu == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize tray icon: required Windows API is unavailable"));
    }
    if (taskbarCreatedMessage == 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize tray icon without TaskbarCreated"));
    }
    if (!quitCallback) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize tray icon without a quit callback"));
    }
    if (taskbarCreatedMessage_ != 0
        && taskbarCreatedMessage_ != taskbarCreatedMessage) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot change TaskbarCreated for an active tray icon"));
    }
    taskbarCreatedMessage_ = taskbarCreatedMessage;
    quitCallback_ = std::move(quitCallback);

    if (messageWindow_ == nullptr || !IsWindow(messageWindow_)) {
        messageWindow_ = nullptr;
        const auto result = createMessageWindow();
        if (!result.hasValue()) {
            return result;
        }
    }
    return registered_ ? Core::Result<void>::success() : addNotification();
}

Core::Result<void> TrayIcon::createMessageWindow()
{
    SetLastError(ERROR_SUCCESS);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (instance == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot obtain the tray module handle (Win32 error %1)")
                .arg(GetLastError()));
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
        return Core::Result<void>::failure(
            QStringLiteral("Cannot create or bind tray message window (Win32 error %1)")
                .arg(GetLastError()));
    }
    return Core::Result<void>::success();
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
    const HICON icon = api_.loadIcon(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (icon == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot load tray icon resource (Win32 error %1)")
                .arg(GetLastError()));
    }

    auto notification = notificationData(messageWindow_, iconId_);
    notification.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    notification.uCallbackMessage = callbackMessage_;
    notification.hIcon = icon;
    constexpr wchar_t tooltip[] = L"StatusBar";
    static_assert(std::size(tooltip) <= std::size(notification.szTip));
    std::copy(std::begin(tooltip), std::end(tooltip), std::begin(notification.szTip));

    if (!api_.notifyIcon(NIM_ADD, &notification)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot add the StatusBar notification icon"));
    }
    registered_ = true;

    notification.uVersion = NOTIFYICON_VERSION_4;
    if (!api_.notifyIcon(NIM_SETVERSION, &notification)) {
        if (!removeNotification()) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot roll back tray icon after NIM_SETVERSION failure"));
        }
        return Core::Result<void>::failure(
            QStringLiteral("Cannot select the tray notification protocol version"));
    }
    return Core::Result<void>::success();
}

bool TrayIcon::removeNotification()
{
    if (!registered_ || messageWindow_ == nullptr) {
        return true;
    }
    auto notification = notificationData(messageWindow_, iconId_);
    registered_ = false;
    return api_.notifyIcon(NIM_DELETE, &notification);
}

Core::Result<void> TrayIcon::recoverAfterShellRestart()
{
    if (taskbarCreatedMessage_ == 0 || !quitCallback_) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot recover tray icon before initialization"));
    }

    registered_ = false;
    if (messageWindow_ == nullptr || !IsWindow(messageWindow_)) {
        messageWindow_ = nullptr;
        const auto result = createMessageWindow();
        if (!result.hasValue()) {
            return result;
        }
    }
    return addNotification();
}

Core::Result<POINT> TrayIcon::notificationAnchor(
    POINT point,
    const bool needsIconRect) const
{
    if (!needsIconRect) {
        return Core::Result<POINT>::success(point);
    }

    NOTIFYICONIDENTIFIER identifier{};
    identifier.cbSize = sizeof(identifier);
    identifier.hWnd = messageWindow_;
    identifier.uID = iconId_;
    RECT iconRect{};
    if (SUCCEEDED(api_.notifyIconRect(&identifier, &iconRect))) {
        point.x = iconRect.left + ((iconRect.right - iconRect.left) / 2);
        point.y = iconRect.bottom;
        return Core::Result<POINT>::success(point);
    }

    SetLastError(ERROR_SUCCESS);
    if (GetCursorPos(&point)) {
        return Core::Result<POINT>::success(point);
    }
    return Core::Result<POINT>::failure(
        QStringLiteral("Cannot locate the tray menu anchor (Win32 error %1)")
            .arg(GetLastError()));
}

Core::Result<void> TrayIcon::showContextMenu(const POINT point)
{
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
    const UINT command = static_cast<UINT>(api_.trackPopupMenu(
        menu.get(),
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        messageWindow_,
        nullptr));
    const DWORD trackingError = GetLastError();
    const DWORD closeError = menu.release();

    SetLastError(ERROR_SUCCESS);
    const bool posted = PostMessageW(messageWindow_, WM_NULL, 0, 0);
    const DWORD postError = GetLastError();

    auto notification = notificationData(messageWindow_, iconId_);
    const bool focusRestored = api_.notifyIcon(NIM_SETFOCUS, &notification);
    const bool earlierFailure = (command == 0 && trackingError != ERROR_SUCCESS)
        || closeError != ERROR_SUCCESS || !posted;
    if (!focusRestored && earlierFailure) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Cannot restore notification-area focus after tray menu"));
    }

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
    if (!focusRestored) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot restore notification-area focus after tray menu"));
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
        if (!removeNotification()) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot remove the StatusBar notification icon"));
        }
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
        return instance == nullptr
            ? DefWindowProcW(window, message, wParam, lParam)
            : instance->handleMessage(window, message, wParam, lParam);
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
    if (message != callbackMessage_) {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    const auto notification = Detail::decodeTrayNotification(wParam, lParam, iconId_);
    if (!notification.has_value()) {
        return 0;
    }

    const auto anchor = notificationAnchor(
        notification->point, notification->needsIconRect);
    const auto result = anchor.hasValue()
        ? showContextMenu(anchor.value())
        : Core::Result<void>::failure(anchor.error());
    if (!result.hasValue()) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Tray menu failed: %1").arg(result.error()));
    }
    return 0;
}

} // namespace Platform
