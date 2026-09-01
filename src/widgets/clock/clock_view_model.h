#pragma once
#include "widgets/i_widget_view_model.h"

namespace Widgets {
    class ClockViewModel : public IWidgetViewModel {
    public:
        ClockViewModel();
        int GetSpan() const override { return 3; }
        std::string GetKind() const override { return "Clock"; }
        void Update() override;
    };
}
