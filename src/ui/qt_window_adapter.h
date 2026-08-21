#pragma once
#include "src/core/i_window_adapter.h"
#include "src/ui/widget_model.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <memory>

namespace UI {
    class QtWindowAdapter : public Core::IWindowAdapter {
    public:
        QtWindowAdapter(int& argc, char** argv);
        ~QtWindowAdapter() override = default;

        void SetDpiScale(float scale) override {}
        void SetSize(uint32_t width, uint32_t height) override {}
        void SetPosition(int x, int y) override {}
        void Show() override {}
        void Run() override;
        void HideFromAltTabAndTaskbar() override;
        void Quit() override;

    private:
        std::unique_ptr<QGuiApplication> m_app;
        std::unique_ptr<QQmlApplicationEngine> m_engine;
        WidgetModel m_widgetModel;
        QTimer m_updateTimer;
    };
}
