#pragma once

#include "core/result.h"

#include <windows.h>

namespace Platform {

[[nodiscard]] RECT topAppBarRect(
    RECT monitorRect,
    RECT shellAdjustedRect,
    LONG desiredHeight) noexcept;

class AppBar final {
public:
    AppBar() = default;
    ~AppBar();

    AppBar(const AppBar&) = delete;
    AppBar& operator=(const AppBar&) = delete;
    AppBar(AppBar&&) = delete;
    AppBar& operator=(AppBar&&) = delete;

    [[nodiscard]] Core::Result<void> initialize(HWND window, int logicalHeight);
    [[nodiscard]] Core::Result<void> reposition();
    [[nodiscard]] Core::Result<void> recoverAfterShellRestart();
    void shutdown() noexcept;

    [[nodiscard]] HWND window() const noexcept { return window_; }
    [[nodiscard]] UINT callbackMessage() const noexcept { return callbackMessage_; }
    [[nodiscard]] UINT taskbarCreatedMessage() const noexcept
    {
        return taskbarCreatedMessage_;
    }

private:
    [[nodiscard]] Core::Result<void> registerWithShell();
    void removeRegistration() noexcept;

    HWND window_{};
    int logicalHeight_{};
    UINT callbackMessage_{};
    UINT taskbarCreatedMessage_{};
    bool registered_{};
};

} // namespace Platform
