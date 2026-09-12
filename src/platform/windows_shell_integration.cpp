#include "platform/windows_shell_integration.h"

#include "platform/windows_logging.h"

#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include <shellapi.h>

#include <cstdlib>
#include <exception>
#include <utility>

namespace Platform {

WindowsShellIntegration::~WindowsShellIntegration()
{
    shutdown();
}

Core::Result<void> WindowsShellIntegration::acquireSingleInstance(const wchar_t* const name)
{
    return singleInstance_.initialize(name);
}

Core::Result<void> WindowsShellIntegration::initialize(
    const HWND rootWindow,
    const int logicalHeight,
    std::function<void()> quitCallback)
{
    if (installed_) {
        return rootWindow_ == rootWindow
            ? Core::Result<void>::success()
            : Core::Result<void>::failure(
                  QStringLiteral("Windows shell integration is already active"));
    }
    auto* const application = QCoreApplication::instance();
    if (application == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot install Windows shell integration without QCoreApplication"));
    }
    if (shellApi_.registerWindowMessage == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot initialize shell integration: RegisterWindowMessageW is unavailable"));
    }

    SetLastError(ERROR_SUCCESS);
    const UINT taskbarCreated = shellApi_.registerWindowMessage(L"TaskbarCreated");
    if (taskbarCreated == 0) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot register TaskbarCreated for shell recovery (Win32 error %1)")
                .arg(GetLastError()));
    }
    auto callbackContext = std::make_unique<QObject>();

    const auto appBarResult = appBar_.initialize(rootWindow, logicalHeight);
    if (!appBarResult.hasValue()) {
        return appBarResult;
    }

    taskbarCreatedMessage_ = taskbarCreated;
    const auto trayResult = trayIcon_.initialize(
        taskbarCreatedMessage_, std::move(quitCallback));
    if (!trayResult.hasValue()) {
        LogWindowsMessage(
            WindowsLogLevel::Warning,
            QStringLiteral("Tray icon is unavailable; continuing without it: %1")
                .arg(trayResult.error()));
    }

    rootWindow_ = rootWindow;
    callbackContext_ = std::move(callbackContext);
    application->installNativeEventFilter(this);
    installed_ = true;
    return Core::Result<void>::success();
}

void WindowsShellIntegration::shutdown() noexcept
{
    try {
        deferredReposition_.cancel();
        if (installed_) {
            if (auto* const application = QCoreApplication::instance()) {
                application->removeNativeEventFilter(this);
            }
            installed_ = false;
        }
        callbackContext_.reset();
        trayIcon_.shutdown();
        appBar_.shutdown();
        fatalExitRequested_ = false;
        rootWindow_ = nullptr;
        taskbarCreatedMessage_ = 0;
    } catch (const std::exception&) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unhandled exception while shutting down Windows shell integration"));
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception while shutting down Windows shell integration"));
    }
}

void WindowsShellIntegration::queueReposition()
{
    const auto token = deferredReposition_.request();
    if (!token.has_value()) {
        return;
    }
    if (callbackContext_ == nullptr) {
        deferredReposition_.cancel();
        return;
    }
    try {
        QTimer::singleShot(
            0,
            callbackContext_.get(),
            [this, value = *token]() noexcept { runQueuedReposition(value); });
    } catch (...) {
        deferredReposition_.cancel();
        throw;
    }
}

void WindowsShellIntegration::runQueuedReposition(
    const Detail::CoalescedCall::Token token) noexcept
{
    try {
        if (!deferredReposition_.consume(token) || !installed_ || fatalExitRequested_) {
            return;
        }
        const auto result = appBar_.reposition();
        if (!result.hasValue()) {
            handleFatalAppBarFailure(
                QStringLiteral("Deferred AppBar display/DPI reposition failed"),
                result.error());
        }
    } catch (...) {
        handleFatalAppBarFailure(
            QStringLiteral("Deferred AppBar reposition raised an exception"),
            QStringLiteral("unknown failure"));
    }
}

void WindowsShellIntegration::handleFatalAppBarFailure(
    const QString& context,
    const QString& error) noexcept
{
    if (fatalExitRequested_) {
        return;
    }
    fatalExitRequested_ = true;
    try {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("%1: %2").arg(context, error));
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Fatal AppBar integration failure"));
    }
    try {
        QTimer::singleShot(0, [] { QCoreApplication::exit(EXIT_FAILURE); });
    } catch (...) {
        QCoreApplication::exit(EXIT_FAILURE);
    }
}

bool WindowsShellIntegration::nativeEventFilter(
    const QByteArray&,
    void* const nativeMessage,
    qintptr*) noexcept
{
    try {
        const auto* const message = static_cast<const MSG*>(nativeMessage);
        if (!installed_ || message == nullptr || message->hwnd != rootWindow_) {
            return false;
        }

        if (message->message == appBar_.callbackMessage()) {
            if (message->wParam == ABN_POSCHANGED) {
                const auto result = appBar_.reposition();
                if (!result.hasValue()) {
                    handleFatalAppBarFailure(
                        QStringLiteral("AppBar position callback failed"), result.error());
                }
            }
            return false;
        }

        if (message->message == WM_DISPLAYCHANGE || message->message == WM_DPICHANGED) {
            queueReposition();
            return false;
        }

        if (message->message == WM_ACTIVATE) {
            appBar_.notifyActivated();
            return false;
        }

        if (message->message == WM_WINDOWPOSCHANGED) {
            appBar_.notifyWindowPosChanged();
            return false;
        }

        if (taskbarCreatedMessage_ != 0
            && message->message == taskbarCreatedMessage_) {
            const auto appBarResult = appBar_.recoverAfterShellRestart();
            if (!appBarResult.hasValue()) {
                handleFatalAppBarFailure(
                    QStringLiteral("AppBar recovery after Explorer restart failed"),
                    appBarResult.error());
            }

            const auto trayResult = trayIcon_.recoverAfterShellRestart();
            if (!trayResult.hasValue()) {
                LogWindowsMessage(
                    WindowsLogLevel::Warning,
                    QStringLiteral("Tray recovery after Explorer restart failed: %1")
                        .arg(trayResult.error()));
            }
            return false;
        }
    } catch (const std::exception&) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unhandled exception in Windows native event filter"));
        QCoreApplication::exit(EXIT_FAILURE);
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown exception in Windows native event filter"));
        QCoreApplication::exit(EXIT_FAILURE);
    }
    return false;
}

} // namespace Platform
