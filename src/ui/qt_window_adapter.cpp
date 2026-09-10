#include "ui/qt_window_adapter.h"
#include "widgets/registry_setup.h"
#include <QCoreApplication>
#include <QDir>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QFont>
#include <stdexcept>

namespace UI {
    QtWindowAdapter::QtWindowAdapter(int& argc, char** argv) {
        m_app = std::make_unique<QGuiApplication>(argc, argv);

        QFont defaultFont("Segoe UI");
        defaultFont.setStyleStrategy(QFont::PreferAntialias);
        m_app->setFont(defaultFont);

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
        m_engine->setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(m_widgetModel.get())},
        });
        m_engine->loadFromModule(QStringLiteral("StatusBar"), QStringLiteral("Main"));
        if (m_engine->rootObjects().isEmpty()
            || qobject_cast<QQuickWindow*>(m_engine->rootObjects().constFirst()) == nullptr) {
            throw std::runtime_error("Failed to load StatusBar.Main");
        }
    }

    void QtWindowAdapter::Run() {
        qobject_cast<QQuickWindow*>(m_engine->rootObjects().constFirst())->show();
        m_app->exec();
    }

    void QtWindowAdapter::Quit() {
        m_app->quit();
    }
}
