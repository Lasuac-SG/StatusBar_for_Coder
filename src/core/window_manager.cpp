#include "src/core/window_manager.h"
#include "src/platform/appbar_proxy.h"
#include <cmath>
#include <utility> // for std::move

namespace Core {
    // 构造函数：接收传入的 UI 适配器，并使用 std::move 将其移动赋值给成员变量。
    WindowManager::WindowManager(std::unique_ptr<IWindowAdapter> uiAdapter) 
        : m_uiAdapter(std::move(uiAdapter)) {
    }

    WindowManager::~WindowManager() {
        Platform::AppBarProxy::Shutdown();
    }

    void WindowManager::DockToTop(int screenWidth, uint32_t dpi) {
        m_physicalWidth = static_cast<uint32_t>(screenWidth);
        m_physicalHeight = static_cast<uint32_t>(std::ceil(m_logicalHeight * dpi / 96.0));
        
        float scaleFactor = static_cast<float>(dpi) / 96.0f;
        
        // 核心新增：全部改为通过抽象接口与前端通信，不再调用具体的 slint api
        m_uiAdapter->SetDpiScale(scaleFactor);
        
        Platform::AppBarProxy::Initialize(m_physicalWidth, m_physicalHeight);

        m_uiAdapter->SetSize(m_physicalWidth, m_physicalHeight);
        m_uiAdapter->SetPosition(0, 0);
    }

    void WindowManager::Show() {
        // 调用接口的显示与运行方法
        m_uiAdapter->Show(); 
        m_uiAdapter->HideFromAltTabAndTaskbar();
        m_uiAdapter->Run();
    }
}
