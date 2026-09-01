#pragma once
#include <string>

namespace Widgets {
    class IWidgetViewModel {
    public:
        virtual ~IWidgetViewModel() = default;
        [[nodiscard]] virtual int GetSpan() const = 0;
        [[nodiscard]] virtual std::string GetKind() const = 0;
        virtual void Update() = 0;
    };
}
