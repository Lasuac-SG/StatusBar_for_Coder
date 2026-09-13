#pragma once

#include "ui/widget_model.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"
#include "widgets/widget_view_model.h"

#include <QtQml/qqmlregistration.h>

struct WidgetModelQmlForeign final {
    Q_GADGET
    QML_FOREIGN(UI::WidgetModel)
    QML_NAMED_ELEMENT(WidgetModel)
    QML_UNCREATABLE("WidgetModel is created by C++")
};

struct WidgetViewModelQmlForeign final {
    Q_GADGET
    QML_FOREIGN(Widgets::WidgetViewModel)
    QML_NAMED_ELEMENT(WidgetViewModel)
    QML_UNCREATABLE("WidgetViewModel is created by C++")
};

struct ClockViewModelQmlForeign final {
    Q_GADGET
    QML_FOREIGN(Widgets::ClockViewModel)
    QML_NAMED_ELEMENT(ClockViewModel)
    QML_UNCREATABLE("ClockViewModel is created by C++")
};

struct CpuViewModelQmlForeign final {
    Q_GADGET
    QML_FOREIGN(Widgets::CpuViewModel)
    QML_NAMED_ELEMENT(CpuViewModel)
    QML_UNCREATABLE("CpuViewModel is created by C++")
};
