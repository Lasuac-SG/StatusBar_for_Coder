#include "src/platform/display.h"
#include <windows.h>

namespace Platform {
    int Display::GetPrimaryScreenWidth() noexcept {
        // 因已开启 DPI Aware，此时返回的必是真实物理宽度 (例如 4K 屏返回 3840)
        return GetSystemMetrics(SM_CXSCREEN);
    }

    uint32_t Display::GetSystemDpi() noexcept {
        return GetDpiForSystem();
    }
}
