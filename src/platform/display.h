#pragma once
#include <cstdint>

namespace Platform {
    class Display {
    public:
        [[nodiscard]] static int GetPrimaryScreenWidth() noexcept;
        [[nodiscard]] static uint32_t GetSystemDpi() noexcept;
    };
}
