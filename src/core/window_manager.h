#pragma once
#include "core/i_window_adapter.h"
#include "platform/tray_icon.h"
#include <cstdint>
#include <memory>

namespace Core {
    class WindowManager {
    public:
        static constexpr uint32_t LOGICAL_BAR_HEIGHT = 40;

        explicit WindowManager(std::unique_ptr<IWindowAdapter> uiAdapter);
        ~WindowManager();

        void DockToTop(int screenWidth, uint32_t systemDpi);
        void Show();

    private:
        std::unique_ptr<IWindowAdapter> m_uiAdapter;
        Platform::TrayIcon m_trayIcon;
    };
}
