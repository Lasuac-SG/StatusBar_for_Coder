#pragma once
#include "main.h"  // Slint 在编译期生成的 UI 头文件
#include <windows.h>
#include <cstdint>

namespace Core {
    class WindowManager {
    public:
        WindowManager();
        ~WindowManager();
        
        void DockToTop(int screenWidth, uint32_t dpi);
        void Show();

    private:
        slint::ComponentHandle<MainWindow> m_uiHandle;
        const uint32_t m_logicalHeight = 48; 

        HWND m_hwnd = nullptr;
        uint32_t m_physicalWidth = 0;
        uint32_t m_physicalHeight = 0;
    };
}
