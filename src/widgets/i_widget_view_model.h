#pragma once

#include <QObject>

#include <string>

namespace Widgets {
    class IWidgetViewModel : public QObject {
    public:
        explicit IWidgetViewModel(QObject* parent = nullptr)
            : QObject(parent)
        {
        }
        virtual ~IWidgetViewModel() = default;
        [[nodiscard]] virtual int GetSpan() const = 0;
        [[nodiscard]] virtual std::string GetKind() const = 0;
        virtual void Update() = 0;
    };
}
