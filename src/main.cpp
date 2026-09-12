#include "core/config_repository.h"
#include "platform/cpu_data_source.h"
#include "platform/single_instance.h"
#include "platform/windows_logging.h"
#include "platform/windows_shell_integration.h"
#include "ui/qt_application.h"
#include "widgets/registry_setup.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QStandardPaths>

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

namespace {

constexpr int logicalBarHeight = 40;
constexpr wchar_t singleInstanceMutexName[] =
    L"Local\\StatusBarForCoder.StatusBar_for_Coder.SingleInstance";

Core::Result<QString> executableDirectory()
{
    constexpr std::size_t maximumModulePathLength = 32768;
    std::vector<wchar_t> buffer(512);

    while (buffer.size() <= maximumModulePathLength) {
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        const DWORD error = GetLastError();
        if (length == 0) {
            return Core::Result<QString>::failure(
                QStringLiteral("Cannot locate the executable (Win32 error %1)").arg(error));
        }
        if (length < buffer.size() && buffer[length] == L'\0') {
            return Core::Result<QString>::success(
                QFileInfo(QString::fromWCharArray(
                              buffer.data(), static_cast<qsizetype>(length)))
                    .absolutePath());
        }
        if (buffer.size() == maximumModulePathLength) {
            break;
        }
        buffer.resize(std::min(buffer.size() * 2, maximumModulePathLength));
    }
    return Core::Result<QString>::failure(
        QStringLiteral("Executable module path exceeds the Windows maximum"));
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
        Platform::SingleInstance singleInstance;
        const auto instanceResult = singleInstance.initialize(singleInstanceMutexName);
        if (!instanceResult.hasValue()) {
            logFatal(instanceResult.error());
            return EXIT_FAILURE;
        }
        if (singleInstance.alreadyRunning()) {
            return EXIT_SUCCESS;
        }

        QCoreApplication::setOrganizationName(QStringLiteral("StatusBarForCoder"));
        QCoreApplication::setApplicationName(QStringLiteral("StatusBar_for_Coder"));

        const QString configDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (configDirectory.isEmpty()) {
            logFatal(QStringLiteral("Qt did not provide an application configuration directory"));
            return EXIT_FAILURE;
        }

        const QString configPath =
            QDir(configDirectory).filePath(QStringLiteral("config.json"));
        QStringList legacyCandidates;
        const auto executableDirectoryResult = executableDirectory();
        if (executableDirectoryResult.hasValue()) {
            legacyCandidates.append(
                QDir(executableDirectoryResult.value()).filePath(QStringLiteral("config.json")));
        } else {
            logWarning(
                QStringLiteral("%1; executable-directory config migration is skipped")
                    .arg(executableDirectoryResult.error()));
        }
        legacyCandidates.append(QDir::current().filePath(QStringLiteral("config.json")));

        auto registryResult = Widgets::registerAllWidgets();
        if (!registryResult.hasValue()) {
            logFatal(
                QStringLiteral("Failed to register widgets: %1").arg(registryResult.error()));
            return EXIT_FAILURE;
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
            return EXIT_FAILURE;
        }

        QQuickWindow* const window = application.window();
        const HWND rootWindow =
            window == nullptr ? nullptr : reinterpret_cast<HWND>(window->winId());
        if (rootWindow == nullptr) {
            logFatal(QStringLiteral("Qt application initialized without a native root window"));
            return EXIT_FAILURE;
        }

        Platform::WindowsShellIntegration shell;
        const auto shellResult = shell.initialize(
            rootWindow, logicalBarHeight, [&application] { application.quit(); });
        if (!shellResult.hasValue()) {
            logFatal(shellResult.error());
            return EXIT_FAILURE;
        }

        const auto showResult = application.show();
        if (!showResult.hasValue()) {
            logFatal(showResult.error());
            return EXIT_FAILURE;
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
    } catch (...) {
        logFatal(QStringLiteral("Unknown startup failure"));
    }
    return EXIT_FAILURE;
}
