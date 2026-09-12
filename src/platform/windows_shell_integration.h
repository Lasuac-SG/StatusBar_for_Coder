#pragma once

#include "core/result.h"
#include "platform/appbar.h"
#include "platform/single_instance.h"
#include "platform/tray_icon.h"

#include <QAbstractNativeEventFilter>

#include <windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace Platform {

namespace Detail {

class CoalescedCall final {
public:
    using Token = std::uint64_t;

    [[nodiscard]] std::optional<Token> request() noexcept
    {
        if (pending_) {
            return std::nullopt;
        }
        pending_ = true;
        return ++generation_;
    }

    [[nodiscard]] bool consume(const Token token) noexcept
    {
        if (!pending_ || token != generation_) {
            return false;
        }
        pending_ = false;
        return true;
    }

    void cancel() noexcept
    {
        pending_ = false;
        ++generation_;
    }

private:
    Token generation_{};
    bool pending_{};
};

} // namespace Detail

class WindowsShellIntegration final : public QAbstractNativeEventFilter {
public:
    explicit WindowsShellIntegration(
        AppBarApi appBarApi = {},
        TrayIconApi trayIconApi = {}) noexcept
        : appBar_(appBarApi)
        , trayIcon_(trayIconApi)
    {
    }
    ~WindowsShellIntegration() override;

    WindowsShellIntegration(const WindowsShellIntegration&) = delete;
    WindowsShellIntegration& operator=(const WindowsShellIntegration&) = delete;
    WindowsShellIntegration(WindowsShellIntegration&&) = delete;
    WindowsShellIntegration& operator=(WindowsShellIntegration&&) = delete;

    [[nodiscard]] Core::Result<void> acquireSingleInstance(const wchar_t* name);
    [[nodiscard]] bool alreadyRunning() const noexcept
    {
        return singleInstance_.alreadyRunning();
    }

    [[nodiscard]] Core::Result<void> initialize(
        HWND rootWindow,
        int logicalHeight,
        std::function<void()> quitCallback);
    void shutdown() noexcept;

    bool nativeEventFilter(
        const QByteArray& eventType,
        void* message,
        qintptr* result) noexcept override;

private:
    void queueReposition();
    void runQueuedReposition(Detail::CoalescedCall::Token token) noexcept;
    void handleFatalAppBarFailure(const QString& context, const QString& error) noexcept;

    SingleInstance singleInstance_;
    HWND rootWindow_{};
    bool installed_{};
    bool fatalExitRequested_{};
    Detail::CoalescedCall deferredReposition_;
    std::unique_ptr<QObject> callbackContext_;
    AppBar appBar_;
    TrayIcon trayIcon_;
};

} // namespace Platform
