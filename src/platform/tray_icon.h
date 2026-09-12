#pragma once

#include "core/result.h"

#include <windows.h>

#include <functional>

namespace Platform {

class TrayIcon final {
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    TrayIcon(TrayIcon&&) = delete;
    TrayIcon& operator=(TrayIcon&&) = delete;

    [[nodiscard]] Core::Result<void> initialize(std::function<void()> quitCallback);
    [[nodiscard]] Core::Result<void> recoverAfterShellRestart();
    void shutdown() noexcept;

    [[nodiscard]] UINT taskbarCreatedMessage() const noexcept
    {
        return taskbarCreatedMessage_;
    }

private:
    [[nodiscard]] Core::Result<void> addNotification();
    [[nodiscard]] Core::Result<void> showContextMenu();
    void destroyMessageWindow() noexcept;

    static LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) noexcept;
    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    static constexpr UINT callbackMessage_ = WM_APP + 0x352;
    static constexpr UINT iconId_ = 1;

    HWND messageWindow_{};
    UINT taskbarCreatedMessage_{};
    bool registered_{};
    std::function<void()> quitCallback_;
};

} // namespace Platform
