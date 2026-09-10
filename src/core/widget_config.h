#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace Core {

struct WidgetConfig final {
    QString id;
    QString type;
    int slot{};
    QJsonObject settings;
    QJsonObject extensions;

    friend bool operator==(const WidgetConfig&, const WidgetConfig&) = default;
};

struct ConfigDocument final {
    int version{1};
    QList<WidgetConfig> widgets;
    QJsonObject extensions;
};

} // namespace Core
