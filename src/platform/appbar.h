#pragma once

#include "core/result.h"

#include <windows.h>
#include <shellapi.h>

namespace Platform {

[[nodiscard]] RECT topAppBarRect(
    RECT monitorRect,
    RECT shellAdjustedRect,
    LONG desiredHeight) noexcept;

struct AppBarApi final {
    decltype(&SHAppBarMessage) message{&SHAppBarMessage};
};

class AppBar final {
public:
    explicit AppBar(AppBarApi api = {}) noexcept
        : api_(api)
    {
    }
    ~AppBar();

    AppBar(const AppBar&) = delete;
    AppBar& operator=(const AppBar&) = delete;
    AppBar(AppBar&&) = delete;
    AppBar& operator=(AppBar&&) = delete;

    [[nodiscard]] Core::Result<void> initialize(HWND window, int logicalHeight);
    [[nodiscard]] Core::Result<void> reposition();
    [[nodiscard]] Core::Result<void> recoverAfterShellRestart();
    void notifyActivated() noexcept;
    void notifyWindowPosChanged() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] HWND window() const noexcept { return window_; }
    [[nodiscard]] UINT callbackMessage() const noexcept { return callbackMessage_; }

private:
    [[nodiscard]] Core::Result<void> registerWithShell();
    void removeRegistration() noexcept;
    void sendShell(DWORD message) noexcept;

    AppBarApi api_;
    HWND window_{};
    int logicalHeight_{};
    UINT callbackMessage_{};
    bool registered_{};
};

} // namespace Platform
