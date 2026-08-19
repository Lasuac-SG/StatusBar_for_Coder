#include "src/core/window_manager.h"
#include "src/platform/appbar_proxy.h"
#include <cmath>

namespace Core {
    WindowManager::WindowManager() 
        : m_uiHandle(MainWindow::create()) {
    }

    WindowManager::~WindowManager() {
        Platform::AppBarProxy::Shutdown();
    }

    void WindowManager::DockToTop(int screenWidth, uint32_t dpi) {
        m_physicalWidth = static_cast<uint32_t>(screenWidth);
        m_physicalHeight = static_cast<uint32_t>(std::ceil(m_logicalHeight * dpi / 96.0));
        
        // [核心新增]: 计算出精准的浮点型倍率 (例如 1.5)，并通过底层桥梁传给 Slint 前端
        float scaleFactor = static_cast<float>(dpi) / 96.0f;
        m_uiHandle->set_dpi_scale(scaleFactor);
        
        Platform::AppBarProxy::Initialize(m_physicalWidth, m_physicalHeight);

        m_uiHandle->window().set_size(slint::PhysicalSize({ m_physicalWidth, m_physicalHeight }));
        m_uiHandle->window().set_position(slint::PhysicalPosition({ 0, 0 }));
    }

    void WindowManager::Show() {
        m_uiHandle->show(); 
        m_uiHandle->run();
    }
}
