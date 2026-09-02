#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Core {
struct LayoutItem final {
    std::string id;
    int slot{};
    int span{1};
    friend bool operator==(const LayoutItem&, const LayoutItem&) = default;
};

class LayoutEngine final {
public:
    [[nodiscard]] static bool isValid(const std::vector<LayoutItem>& items, int totalSlots) noexcept;
    [[nodiscard]] static std::vector<LayoutItem> normalize(std::vector<LayoutItem> items, int totalSlots);
    [[nodiscard]] static std::optional<std::vector<LayoutItem>> drop(
        const std::vector<LayoutItem>& items,
        std::string_view draggedId,
        int targetSlot,
        int totalSlots);
};
}
