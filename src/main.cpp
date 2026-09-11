#include "core/config_repository.h"
#include "platform/appbar_proxy.h"
#include "platform/cpu_data_source.h"
#include "platform/display.h"
#include "platform/tray_icon.h"
#include "platform/windows_logging.h"
#include "ui/qt_application.h"
#include "widgets/registry_setup.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QStandardPaths>

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t logicalBarHeight = 40;
constexpr wchar_t singleInstanceMutexName[] =
    L"Local\\StatusBarForCoder.StatusBar_for_Coder.SingleInstance";

class ScopedHandle final {
public:
    explicit ScopedHandle(HANDLE handle) noexcept
        : handle_(handle)
    {
    }

    ~ScopedHandle()
    {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&&) = delete;
    ScopedHandle& operator=(ScopedHandle&&) = delete;

private:
    HANDLE handle_;
};

Core::Result<QString> executableDirectory()
{
    constexpr std::size_t maximumModulePathLength = 32768;
    std::vector<wchar_t> buffer(512);

    while (buffer.size() <= maximumModulePathLength) {
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));
        const DWORD error = GetLastError();
        if (length == 0) {
            return Core::Result<QString>::failure(
                QStringLiteral(
                    "GetModuleFileNameW failed while locating the executable "
                    "(Win32 error %1)")
                    .arg(error));
        }
        if (length < buffer.size() && buffer[length] == L'\0') {
            const QString modulePath =
                QString::fromWCharArray(buffer.data(), static_cast<qsizetype>(length));
            return Core::Result<QString>::success(
                QFileInfo(modulePath).absolutePath());
        }
        if (buffer.size() == maximumModulePathLength) {
            return Core::Result<QString>::failure(
                QStringLiteral(
                    "Executable module path exceeds the Windows maximum "
                    "(Win32 error %1)")
                    .arg(error));
        }

        buffer.resize(std::min(buffer.size() * 2, maximumModulePathLength));
    }

    return Core::Result<QString>::failure(
        QStringLiteral("Cannot determine the executable module directory"));
}

void logFatal(const QString& message)
{
    Platform::LogWindowsMessage(Platform::WindowsLogLevel::Error, message);
}

void logWarning(const QString& message)
{
    Platform::LogWindowsMessage(Platform::WindowsLogLevel::Warning, message);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        QCoreApplication::setOrganizationName(QStringLiteral("StatusBarForCoder"));
        QCoreApplication::setApplicationName(QStringLiteral("StatusBar_for_Coder"));

        SetLastError(ERROR_SUCCESS);
        const HANDLE mutexHandle =
            CreateMutexW(nullptr, FALSE, singleInstanceMutexName);
        const DWORD mutexStatus = GetLastError();
        if (mutexHandle == nullptr) {
            logFatal(
                QStringLiteral(
                    "Cannot create the single-instance mutex (Win32 error %1)")
                    .arg(mutexStatus));
            return 1;
        }
        const ScopedHandle singleInstanceMutex(mutexHandle);
        if (mutexStatus == ERROR_ALREADY_EXISTS) {
            return 0;
        }
        if (mutexStatus != ERROR_SUCCESS) {
            logFatal(
                QStringLiteral(
                    "Single-instance mutex returned unexpected Win32 status %1")
                    .arg(mutexStatus));
            return 1;
        }

        const QString configDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (configDirectory.isEmpty()) {
            logFatal(QStringLiteral("Qt did not provide an application configuration directory"));
            return 1;
        }

        const QString configPath =
            QDir(configDirectory).filePath(QStringLiteral("config.json"));
        QStringList legacyCandidates;
        const auto executableDirectoryResult = executableDirectory();
        if (executableDirectoryResult.hasValue()) {
            legacyCandidates.append(
                QDir(executableDirectoryResult.value())
                    .filePath(QStringLiteral("config.json")));
        } else {
            logWarning(
                QStringLiteral("%1; executable-directory config migration is skipped")
                    .arg(executableDirectoryResult.error()));
        }
        legacyCandidates.append(
            QDir::current().filePath(QStringLiteral("config.json")));
        auto registryResult = Widgets::registerAllWidgets();
        if (!registryResult.hasValue()) {
            logFatal(
                QStringLiteral("Failed to register widgets: %1").arg(registryResult.error()));
            return 1;
        }

        UI::QtApplication application(
            argc,
            argv,
            Core::ConfigRepository(configPath, legacyCandidates),
            std::move(registryResult).value(),
            std::make_unique<Platform::WindowsCpuDataSource>());
        const auto initializeResult = application.initialize();
        if (!initializeResult.hasValue()) {
            logFatal(initializeResult.error());
            return 1;
        }

        QQuickWindow* const window = application.window();
        if (window == nullptr) {
            logFatal(QStringLiteral("Qt application initialized without a root window"));
            return 1;
        }
        const HWND rootHwnd = reinterpret_cast<HWND>(window->winId());
        if (rootHwnd == nullptr) {
            logFatal(QStringLiteral("Failed to obtain the native handle for the root window"));
            return 1;
        }

        const int screenWidth = Platform::Display::GetPrimaryScreenWidth();
        const std::uint32_t systemDpi = Platform::Display::GetSystemDpi();
        if (screenWidth <= 0 || systemDpi == 0) {
            logFatal(QStringLiteral("Cannot determine primary display dimensions or DPI"));
            return 1;
        }
        const auto physicalHeight = static_cast<std::uint32_t>(
            std::ceil(logicalBarHeight * (systemDpi / 96.0)));
        const auto appBarResult = Platform::AppBarProxy::Initialize(
            rootHwnd, static_cast<std::uint32_t>(screenWidth), physicalHeight);
        if (!appBarResult.hasValue()) {
            logFatal(appBarResult.error());
            return 1;
        }
        const auto appBarCleanup = qScopeGuard([] { Platform::AppBarProxy::Shutdown(); });

        Platform::TrayIcon trayIcon;
        trayIcon.SetQuitCallback([&application] { application.quit(); });
        const auto trayResult = trayIcon.Initialize();
        if (!trayResult.hasValue()) {
            logWarning(trayResult.error());
        }

        const auto showResult = application.show();
        if (!showResult.hasValue()) {
            logFatal(showResult.error());
            return 1;
        }
        return application.run();
    } catch (const std::exception& error) {
        try {
            logFatal(
                QStringLiteral("Unhandled startup exception: %1")
                    .arg(QString::fromUtf8(error.what())));
        } catch (...) {
            logFatal(QStringLiteral("Unhandled startup exception"));
        }
        return 1;
    } catch (...) {
        logFatal(QStringLiteral("Unknown startup failure"));
        return 1;
    }
}
