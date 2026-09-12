#pragma once

#include "core/result.h"

#include <windows.h>
#include <shellapi.h>

#include <functional>
#include <optional>

namespace Platform {

namespace Detail {

struct TrayNotification final {
    UINT notification{};
    POINT point{};
    bool needsIconRect{};
};

[[nodiscard]] std::optional<TrayNotification> decodeTrayNotification(
    WPARAM packedPoint,
    LPARAM packedNotification,
    UINT iconId) noexcept;

} // namespace Detail

struct TrayIconApi final {
    decltype(&Shell_NotifyIconW) notifyIcon{&Shell_NotifyIconW};
    decltype(&Shell_NotifyIconGetRect) notifyIconRect{&Shell_NotifyIconGetRect};
    decltype(&LoadIconW) loadIcon{&LoadIconW};
};

class TrayIcon final {
public:
    explicit TrayIcon(TrayIconApi api = {}) noexcept
        : api_(api)
    {
    }
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
    [[nodiscard]] Core::Result<void> createMessageWindow();
    [[nodiscard]] Core::Result<void> addNotification();
    [[nodiscard]] Core::Result<POINT> notificationAnchor(
        POINT point,
        bool needsIconRect) const;
    [[nodiscard]] Core::Result<void> showContextMenu(POINT point);
    [[nodiscard]] bool removeNotification();
    void destroyMessageWindow() noexcept;

    static LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) noexcept;
    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    static constexpr UINT callbackMessage_ = WM_APP + 0x352;
    static constexpr UINT iconId_ = 1;

    TrayIconApi api_;
    HWND messageWindow_{};
    UINT taskbarCreatedMessage_{};
    bool registered_{};
    std::function<void()> quitCallback_;
};

} // namespace Platform
