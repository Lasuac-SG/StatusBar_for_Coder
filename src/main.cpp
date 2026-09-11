#include "core/config_repository.h"
#include "platform/appbar_proxy.h"
#include "platform/cpu_data_source.h"
#include "platform/display.h"
#include "platform/tray_icon.h"
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
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::uint32_t logicalBarHeight = 40;

QString executableDirectory(const int argc, char** argv)
{
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
        return QDir::currentPath();
    }
    return QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
}

void logFatal(const QString& message)
{
    std::cerr << "[Fatal Error] " << message.toStdString() << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        QCoreApplication::setOrganizationName(QStringLiteral("StatusBarForCoder"));
        QCoreApplication::setApplicationName(QStringLiteral("StatusBar_for_Coder"));

        const QString configDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (configDirectory.isEmpty()) {
            logFatal(QStringLiteral("Qt did not provide an application configuration directory"));
            return 1;
        }

        const QString configPath =
            QDir(configDirectory).filePath(QStringLiteral("config.json"));
        const QStringList legacyCandidates{
            QDir(executableDirectory(argc, argv)).filePath(QStringLiteral("config.json")),
            QDir::current().filePath(QStringLiteral("config.json")),
        };
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
            std::cerr << "[Warning] " << trayResult.error().toStdString() << '\n';
        }

        const auto showResult = application.show();
        if (!showResult.hasValue()) {
            logFatal(showResult.error());
            return 1;
        }
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "[Fatal Error] " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "[Fatal Error] Unknown startup failure\n";
        return 1;
    }
}
