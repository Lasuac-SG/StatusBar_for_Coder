#pragma once
#include "src/ui/viewmodels/i_view_model.h"

namespace UI {
    class ClockViewModel : public IViewModel {
    public:
        ClockViewModel();
        int GetSpan() const override { return 3; }
        std::string GetName() const override { return "Clock"; }
        void Update() override;
    };
}
