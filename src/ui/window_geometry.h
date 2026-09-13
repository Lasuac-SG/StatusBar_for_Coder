#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QRectF>
#include <QtQmlIntegration/qqmlintegration.h>

namespace UI {

class WindowGeometry final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(WindowGeometry)
    QML_SINGLETON

public:
    explicit WindowGeometry(QObject* parent = nullptr);

    Q_INVOKABLE QRectF availableGeometry(QQuickWindow* window) const noexcept;
};

} // namespace UI
