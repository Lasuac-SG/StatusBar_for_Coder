#include "platform/appbar.h"

namespace Platform {

RECT topAppBarRect(
    const RECT monitorRect,
    const RECT shellAdjustedRect,
    const LONG desiredHeight) noexcept
{
    return {
        monitorRect.left,
        shellAdjustedRect.top,
        monitorRect.right,
        shellAdjustedRect.top + desiredHeight,
    };
}

AppBar::~AppBar()
{
    shutdown();
}

Core::Result<void> AppBar::initialize(const HWND window, const int logicalHeight)
{
    if (api_.message == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize AppBar: SHAppBarMessage is unavailable"));
    }
    if (registered_) {
        if (window_ == window && logicalHeight_ == logicalHeight) {
            return Core::Result<void>::success();
        }
        return Core::Result<void>::failure(
            QStringLiteral("Cannot reinitialize an active AppBar"));
    }
    if (window == nullptr || !IsWindow(window)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize AppBar: root HWND is invalid"));
    }
    if (logicalHeight <= 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize AppBar: logical height must be positive"));
    }

    SetLastError(ERROR_SUCCESS);
    const UINT callback = RegisterWindowMessageW(
        L"StatusBarForCoder.AppBarCallback");
    if (callback == 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot register the AppBar callback message (Win32 error %1)")
                .arg(GetLastError()));
    }

    window_ = window;
    logicalHeight_ = logicalHeight;
    callbackMessage_ = callback;

    const auto result = registerWithShell();
    if (!result.hasValue()) {
        shutdown();
    }
    return result;
}

Core::Result<void> AppBar::registerWithShell()
{
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uCallbackMessage = callbackMessage_;
    data.uEdge = ABE_TOP;
    if (api_.message(ABM_NEW, &data) == 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot register AppBar with the Windows shell (ABM_NEW failed)"));
    }

    registered_ = true;
    const auto result = reposition();
    if (!result.hasValue()) {
        removeRegistration();
    }
    return result;
}

Core::Result<void> AppBar::reposition()
{
    if (!registered_ || window_ == nullptr || !IsWindow(window_)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot position AppBar: registration or root HWND is invalid"));
    }

    SetLastError(ERROR_SUCCESS);
    const HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot find the AppBar monitor (Win32 error %1)")
                .arg(GetLastError()));
    }

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    SetLastError(ERROR_SUCCESS);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot query the AppBar monitor (Win32 error %1)")
                .arg(GetLastError()));
    }

    SetLastError(ERROR_SUCCESS);
    const UINT dpi = GetDpiForWindow(window_);
    if (dpi == 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot query the AppBar window DPI (Win32 error %1)")
                .arg(GetLastError()));
    }
    const int physicalHeight = MulDiv(logicalHeight_, static_cast<int>(dpi), 96);
    if (physicalHeight <= 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot convert the AppBar height for DPI %1").arg(dpi));
    }

    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uEdge = ABE_TOP;
    data.rc = topAppBarRect(
        monitorInfo.rcMonitor,
        monitorInfo.rcMonitor,
        static_cast<LONG>(physicalHeight));
    api_.message(ABM_QUERYPOS, &data);

    data.rc = topAppBarRect(
        monitorInfo.rcMonitor, data.rc, static_cast<LONG>(physicalHeight));
    // ABM_SETPOS reports its approved rectangle through data.rc and is documented
    // to always return TRUE.
    api_.message(ABM_SETPOS, &data);

    SetLastError(ERROR_SUCCESS);
    if (!SetWindowPos(
            window_,
            nullptr,
            data.rc.left,
            data.rc.top,
            data.rc.right - data.rc.left,
            data.rc.bottom - data.rc.top,
            SWP_NOACTIVATE | SWP_NOZORDER)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot position the root AppBar window (Win32 error %1)")
                .arg(GetLastError()));
    }

    return Core::Result<void>::success();
}

Core::Result<void> AppBar::recoverAfterShellRestart()
{
    if (window_ == nullptr || !IsWindow(window_)) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot recover AppBar: root HWND is invalid"));
    }

    sendShell(ABM_REMOVE);
    registered_ = false;
    return registerWithShell();
}

void AppBar::notifyActivated() noexcept
{
    if (registered_) {
        sendShell(ABM_ACTIVATE);
    }
}

void AppBar::notifyWindowPosChanged() noexcept
{
    if (registered_) {
        sendShell(ABM_WINDOWPOSCHANGED);
    }
}

void AppBar::sendShell(const DWORD message) noexcept
{
    if (window_ == nullptr) {
        return;
    }
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    api_.message(message, &data);
}

void AppBar::removeRegistration() noexcept
{
    if (!registered_) {
        return;
    }

    sendShell(ABM_REMOVE);
    registered_ = false;
}

void AppBar::shutdown() noexcept
{
    removeRegistration();
    window_ = nullptr;
    logicalHeight_ = 0;
    callbackMessage_ = 0;
}

} // namespace Platform
