#pragma once

#include <QObject>
#include <QString>

namespace Widgets {

class WidgetViewModel : public QObject {
    Q_OBJECT

public:
    explicit WidgetViewModel(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~WidgetViewModel() override = default;

    WidgetViewModel(const WidgetViewModel&) = delete;
    WidgetViewModel& operator=(const WidgetViewModel&) = delete;
    WidgetViewModel(WidgetViewModel&&) = delete;
    WidgetViewModel& operator=(WidgetViewModel&&) = delete;

    [[nodiscard]] virtual const QString& instanceId() const noexcept = 0;
};

} // namespace Widgets
