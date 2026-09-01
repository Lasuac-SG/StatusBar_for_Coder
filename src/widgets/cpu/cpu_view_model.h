#pragma once
#include "widgets/i_widget_view_model.h"
#include "widgets/cpu/cpu_adapter.h"

namespace Widgets {
    class CpuViewModel : public IWidgetViewModel {
    public:
        CpuViewModel() {
            CpuAdapter::GetInstance().Update();
        }

        int GetSpan() const override { return 2; }
        std::string GetKind() const override { return "Cpu"; }

        void Update() override {
            if (++m_ticks >= 2) {
                m_ticks = 0;
                CpuAdapter::GetInstance().Update();
            }
        }

    private:
        int m_ticks = 0;
    };
}
