#pragma once

#include "core/result.h"
#include "platform/appbar.h"
#include "platform/tray_icon.h"

#include <QAbstractNativeEventFilter>

#include <windows.h>

#include <functional>

namespace Platform {

class WindowsShellIntegration final : public QAbstractNativeEventFilter {
public:
    WindowsShellIntegration() = default;
    ~WindowsShellIntegration() override;

    WindowsShellIntegration(const WindowsShellIntegration&) = delete;
    WindowsShellIntegration& operator=(const WindowsShellIntegration&) = delete;
    WindowsShellIntegration(WindowsShellIntegration&&) = delete;
    WindowsShellIntegration& operator=(WindowsShellIntegration&&) = delete;

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
    void handleFatalAppBarFailure(const QString& context, const QString& error) noexcept;

    HWND rootWindow_{};
    bool installed_{};
    bool trayAvailable_{};
    bool fatalExitRequested_{};
    AppBar appBar_;
    TrayIcon trayIcon_;
};

} // namespace Platform
