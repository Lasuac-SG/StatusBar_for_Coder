#pragma once
#include "src/core/i_window_adapter.h"
#include "src/ui/viewmodels/i_view_model.h"
#include "main.h" 
#include <windows.h>
#include <vector>
#include <memory>

namespace UI {
    class SlintWindowAdapter : public Core::IWindowAdapter {
    public:
        SlintWindowAdapter();
        ~SlintWindowAdapter() override = default;

        void SetDpiScale(float scale) override;
        void SetSize(uint32_t width, uint32_t height) override;
        void SetPosition(int x, int y) override;
        void Show() override;
        void Run() override;
        void HideFromAltTabAndTaskbar() override;
        void Quit() override;

    private:
        slint::ComponentHandle<MainWindow> m_uiHandle;
        
        // [核心修复]：确保定时器被明确声明
        slint::Timer m_updateTimer; 
        
        std::vector<std::unique_ptr<IViewModel>> m_viewModels;
        
        HWND GetHwnd() const;
    };
}
