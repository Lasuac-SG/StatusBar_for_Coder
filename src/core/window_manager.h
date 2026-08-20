#pragma once
#include "src/core/i_window_adapter.h" // 仅仅引入抽象接口，彻底斩断对具体 UI 框架的依赖
#include "src/platform/tray_icon.h"
#include <windows.h>
#include <cstdint>
#include <memory> // 引入智能指针

namespace Core {
    class WindowManager {
    public:
        // 依赖注入：要求外部在构造时传入一个实现了 IWindowAdapter 的具体实例。
        // 使用 std::unique_ptr 明确所有权转移，WindowManager 负责管理该实例的生命周期。
        explicit WindowManager(std::unique_ptr<IWindowAdapter> uiAdapter);
        ~WindowManager();
        
        void DockToTop(int screenWidth, uint32_t dpi);
        void Show();

    private:
        // 核心层现在只持有一个抽象接口的指针，对 Slint 一无所知。
        std::unique_ptr<IWindowAdapter> m_uiAdapter;
        Platform::TrayIcon m_trayIcon;
        const uint32_t m_logicalHeight = 48; 
        uint32_t m_physicalWidth = 0;
        uint32_t m_physicalHeight = 0;
    };
}
