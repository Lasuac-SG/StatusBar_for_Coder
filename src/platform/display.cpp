#include "platform/display.h"
#include <windows.h>

namespace Platform {
    int Display::GetPrimaryScreenWidth() noexcept {
        return GetSystemMetrics(SM_CXSCREEN);
    }

    uint32_t Display::GetSystemDpi() noexcept {
        return GetDpiForSystem();
    }
}
