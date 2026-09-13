#include "ui/window_geometry.h"

#include <QQuickWindow>
#include <QScreen>

namespace UI {

WindowGeometry::WindowGeometry(QObject* parent)
    : QObject(parent)
{
}

QRectF WindowGeometry::availableGeometry(QQuickWindow* window) const noexcept
{
    if (window == nullptr || window->screen() == nullptr) {
        return {};
    }
    return window->screen()->availableGeometry();
}

} // namespace UI
