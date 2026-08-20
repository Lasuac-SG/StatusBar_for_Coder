#pragma once
#include "src/ui/viewmodels/i_view_model.h"
#include "main.h" // 需要知道 MainWindow

namespace UI {
    class ClockViewModel : public IViewModel {
    public:
        // [核心修复]：采用显式构造函数进行依赖注入
        explicit ClockViewModel(const slint::ComponentHandle<MainWindow>& uiHandle);
        
        void Update() override;

    private:
        // 此变量现在会在构造时被直接初始化，合法合规
        slint::ComponentHandle<MainWindow> m_uiHandle;
    };
}
