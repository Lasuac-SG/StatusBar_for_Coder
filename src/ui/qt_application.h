#pragma once

#include "core/config_repository.h"
#include "core/result.h"
#include "platform/cpu_data_source.h"
#include "platform/cpu_service.h"
#include "ui/widget_model.h"
#include "widgets/widget_descriptor.h"
#include "widgets/widget_registry.h"

#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QString>
#include <QStringList>

#include <memory>

class QQuickWindow;

namespace UI {

struct QmlEntryPoint final {
    QString moduleUri{QStringLiteral("StatusBar")};
    QString typeName{QStringLiteral("Main")};
};

class QtApplication final {
public:
    QtApplication(
        int& argc,
        char** argv,
        Core::ConfigRepository repository,
        Widgets::WidgetRegistry registry,
        std::unique_ptr<Platform::CpuDataSource> cpuDataSource,
        QmlEntryPoint qmlEntryPoint = {});
    ~QtApplication();

    QtApplication(const QtApplication&) = delete;
    QtApplication& operator=(const QtApplication&) = delete;
    QtApplication(QtApplication&&) = delete;
    QtApplication& operator=(QtApplication&&) = delete;

    [[nodiscard]] Core::Result<void> initialize();
    [[nodiscard]] QQuickWindow* window() const noexcept;
    [[nodiscard]] Core::Result<void> show();
    [[nodiscard]] int run();
    void quit() noexcept;

private:
    enum class State {
        Uninitialized,
        Initializing,
        Initialized,
        Failed,
    };

    [[nodiscard]] Core::Result<void> failInitialization(QString error);
    void discardRootObjects() noexcept;

    QGuiApplication application_;
    Platform::CpuService cpuService_;
    Widgets::WidgetContext widgetContext_;
    QString configPath_;
    WidgetModel widgetModel_;
    QmlEntryPoint qmlEntryPoint_;
    QStringList qmlDiagnostics_;
    QString failure_;
    State state_{State::Uninitialized};
    bool objectCreationFailed_{};
    QQmlApplicationEngine engine_;
    QPointer<QQuickWindow> rootWindow_;
};

} // namespace UI
