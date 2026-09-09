#pragma once

#include "core/result.h"
#include "widgets/widget_registry.h"

namespace Widgets {

[[nodiscard]] Core::Result<WidgetRegistry> registerAllWidgets();

} // namespace Widgets
