#include "src/core/window_manager.h"
#include "src/platform/appbar_proxy.h"
#include <cmath>
#include <utility>

namespace Core {
    WindowManager::WindowManager(std::unique_ptr<IWindowAdapter> uiAdapter) 
        : m_uiAdapter(std::move(uiAdapter)) {}

    WindowManager::~WindowManager() {
        Platform::AppBarProxy::Shutdown();
    }

    void WindowManager::DockToTop(int screenWidth, uint32_t systemDpi) {
        uint32_t physicalWidth = static_cast<uint32_t>(screenWidth);
        uint32_t physicalHeight = static_cast<uint32_t>(std::ceil(LOGICAL_BAR_HEIGHT * (systemDpi / 96.0)));

        Platform::AppBarProxy::Initialize(physicalWidth, physicalHeight);

        m_trayIcon.SetQuitCallback([this]() {
            this->m_uiAdapter->Quit();
        });
        m_trayIcon.Initialize();
    }

    void WindowManager::Show() {
        m_uiAdapter->Run();
    }
}
