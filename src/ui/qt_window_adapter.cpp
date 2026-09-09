#include "ui/qt_window_adapter.h"
#include "core/window_manager.h"
#include "widgets/registry_setup.h"
#include <QCoreApplication>
#include <QDir>
#include <QQmlContext>
#include <QStandardPaths>
#include <QtQml>
#include <QFont>
#include <iostream>
#include <stdexcept>

namespace UI {
    QtWindowAdapter::QtWindowAdapter(int& argc, char** argv) {
        m_app = std::make_unique<QGuiApplication>(argc, argv);

        QFont defaultFont("Segoe UI");
        defaultFont.setStyleStrategy(QFont::PreferAntialias);
        m_app->setFont(defaultFont);

        // 注册全局 Theme 1.0 单例
        qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/src/ui/qml/Theme.qml")), "Theme", 1, 0, "Theme");

        m_engine = std::make_unique<QQmlApplicationEngine>();

        const QString configPath =
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                .filePath(QStringLiteral("config.json"));
        const QStringList legacyCandidates{
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.json")),
            QDir::current().filePath(QStringLiteral("config.json")),
        };
        auto registry = Widgets::registerAllWidgets();
        if (!registry.hasValue()) {
            throw std::runtime_error(
                QStringLiteral("Failed to register widgets: %1")
                    .arg(registry.error())
                    .toStdString());
        }
        m_cpuService = std::make_unique<Platform::CpuService>(
            std::make_unique<Platform::WindowsCpuDataSource>());
        Widgets::WidgetContext widgetContext{*m_cpuService};
        m_widgetModel = std::make_unique<WidgetModel>(
            Core::ConfigRepository(configPath, legacyCandidates),
            std::move(registry).value(),
            widgetContext);
        const auto configResult = m_widgetModel->loadFromConfig();
        if (!configResult.hasValue()) {
            throw std::runtime_error(
                QStringLiteral("Failed to load configuration from %1: %2")
                    .arg(configPath, configResult.error())
                    .toStdString());
        }
        m_cpuService->start();

        m_engine->rootContext()->setContextProperty("reservedBarHeight", static_cast<int>(Core::WindowManager::LOGICAL_BAR_HEIGHT));
        m_engine->rootContext()->setContextProperty("widgetModel", m_widgetModel.get());

        QObject::connect(m_engine.get(), &QQmlApplicationEngine::objectCreated,
                         m_app.get(), [](QObject *obj, const QUrl &objUrl) {
            if (!obj) {
                std::cerr << "[Fatal Error] 无法加载 QML 视图: " 
                          << objUrl.toString().toStdString() << std::endl;
            }
        }, Qt::DirectConnection);

        const QUrl url(QStringLiteral("qrc:/src/ui/qml/main.qml"));
        m_engine->load(url);
    }

    void QtWindowAdapter::Run() {
        m_app->exec();
    }

    void QtWindowAdapter::Quit() {
        m_app->quit();
    }
}
