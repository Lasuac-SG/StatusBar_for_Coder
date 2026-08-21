#include "src/ui/qt_window_adapter.h"
#include "src/ui/clock_adapter.h"
#include <QQmlContext>

namespace UI {
    QtWindowAdapter::QtWindowAdapter(int& argc, char** argv) {
        m_app = std::make_unique<QGuiApplication>(argc, argv);
        m_engine = std::make_unique<QQmlApplicationEngine>();

        m_widgetModel.loadFromConfig();

        // 注册上下文属性，QML 可直接使用
        m_engine->rootContext()->setContextProperty("widgetModel", &m_widgetModel);
        m_engine->rootContext()->setContextProperty("clockAdapter", &ClockAdapter::GetInstance());

        const QUrl url(u"qrc:/src/ui/qml/main.qml"_qs);
        m_engine->load(url);

        QObject::connect(&m_updateTimer, &QTimer::timeout, [&]() {
            m_widgetModel.updateAll();
        });
        m_updateTimer.start(1000);
    }

    void QtWindowAdapter::Run() {
        m_app->exec();
    }
    
    void QtWindowAdapter::Quit() {
        m_app->quit();
    }

    void QtWindowAdapter::HideFromAltTabAndTaskbar() {
        // Qt 在 QML Window 级别通过 flags 控制，无需此处的 Win32 API 介入
    }
}
