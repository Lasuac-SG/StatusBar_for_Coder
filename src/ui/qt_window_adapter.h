#pragma once
#include "core/i_window_adapter.h"
#include "ui/widget_model.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <memory>

namespace UI {
    class QtWindowAdapter : public Core::IWindowAdapter {
    public:
        QtWindowAdapter(int& argc, char** argv);
        ~QtWindowAdapter() override = default;

        void Run() override;
        void Quit() override;

    private:
        std::unique_ptr<QGuiApplication> m_app;
        std::unique_ptr<QQmlApplicationEngine> m_engine;
        std::unique_ptr<WidgetModel> m_widgetModel;
    };
}
