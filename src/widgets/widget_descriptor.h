#pragma once

#include "core/widget_config.h"
#include "widgets/widget_view_model.h"

#include <QString>
#include <QUrl>

#include <functional>
#include <memory>

namespace Platform {
class CpuService;
}

namespace Widgets {

struct WidgetContext final {
    Platform::CpuService& cpuService;
};

using WidgetFactory = std::function<std::unique_ptr<WidgetViewModel>(
    const Core::WidgetConfig&,
    WidgetContext&)>;

struct WidgetDescriptor final {
    QString type;
    int span{};
    QUrl qmlUrl;
    WidgetFactory create;
};

} // namespace Widgets
