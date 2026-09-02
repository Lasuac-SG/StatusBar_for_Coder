#include "core/layout_engine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>

namespace Core {
namespace {
[[nodiscard]] bool overlaps(int leftSlot, int leftSpan, int rightSlot, int rightSpan) noexcept
{
    const auto leftEnd = static_cast<std::int64_t>(leftSlot) + leftSpan;
    const auto rightEnd = static_cast<std::int64_t>(rightSlot) + rightSpan;
    return leftSlot < rightEnd && rightSlot < leftEnd;
}

[[nodiscard]] bool hasValidIdentityAndSpan(const std::vector<LayoutItem>& items) noexcept
{
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (items[index].span <= 0) {
            return false;
        }

        for (std::size_t otherIndex = 0; otherIndex < index; ++otherIndex) {
            if (items[index].id == items[otherIndex].id) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool hasUniqueIdsAndPositiveSpans(const std::vector<LayoutItem>& items)
{
    std::vector<std::string_view> ids;
    ids.reserve(items.size());
    for (const auto& item : items) {
        if (item.span <= 0) {
            return false;
        }
        ids.emplace_back(item.id);
    }

    std::sort(ids.begin(), ids.end());
    return std::adjacent_find(ids.begin(), ids.end()) == ids.end();
}
}

bool LayoutEngine::isValid(const std::vector<LayoutItem>& items, int totalSlots) noexcept
{
    if (totalSlots < 0 || !hasValidIdentityAndSpan(items)) {
        return false;
    }

    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        if (item.slot < 0 || item.span > totalSlots || item.slot > totalSlots - item.span) {
            return false;
        }

        for (std::size_t otherIndex = 0; otherIndex < index; ++otherIndex) {
            const auto& other = items[otherIndex];
            if (overlaps(item.slot, item.span, other.slot, other.span)) {
                return false;
            }
        }
    }
    return true;
}

std::vector<LayoutItem> LayoutEngine::normalize(std::vector<LayoutItem> items, int totalSlots)
{
    if (totalSlots < 0 || !hasUniqueIdsAndPositiveSpans(items)) {
        return {};
    }

    const auto availableSlots = static_cast<std::int64_t>(totalSlots);
    std::int64_t requiredSlots = 0;
    for (const auto& item : items) {
        requiredSlots += item.span;
        if (requiredSlots > availableSlots) {
            return {};
        }
    }

    std::int64_t previousEnd = 0;
    std::int64_t remainingSpanAfter = requiredSlots;
    for (auto& item : items) {
        remainingSpanAfter -= item.span;
        const auto lastFeasibleSlot = availableSlots - item.span - remainingSpanAfter;
        const auto selectedSlot = std::clamp(
            static_cast<std::int64_t>(item.slot), previousEnd, lastFeasibleSlot);
        if (selectedSlot < std::numeric_limits<int>::min()
            || selectedSlot > std::numeric_limits<int>::max()) {
            return {};
        }

        item.slot = static_cast<int>(selectedSlot);
        previousEnd = selectedSlot + item.span;
    }
    return items;
}

std::optional<std::vector<LayoutItem>> LayoutEngine::drop(
    const std::vector<LayoutItem>& items,
    std::string_view draggedId,
    int targetSlot,
    int totalSlots)
{
    if (!isValid(items, totalSlots)) {
        return std::nullopt;
    }

    const auto dragged = std::find_if(items.begin(), items.end(), [draggedId](const LayoutItem& item) {
        return item.id == draggedId;
    });
    if (dragged == items.end()) {
        return std::nullopt;
    }

    const auto draggedIndex = static_cast<std::size_t>(std::distance(items.begin(), dragged));
    const int oldSlot = dragged->slot;
    const int clampedTarget = std::clamp(targetSlot, 0, totalSlots - dragged->span);
    std::optional<std::size_t> displacedIndex;

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index == draggedIndex) {
            continue;
        }
        const auto& item = items[index];
        if (!overlaps(clampedTarget, dragged->span, item.slot, item.span)) {
            continue;
        }
        if (displacedIndex.has_value()) {
            return std::nullopt;
        }
        displacedIndex = index;
    }

    auto result = items;
    result[draggedIndex].slot = clampedTarget;
    if (displacedIndex.has_value()) {
        auto& displaced = result[*displacedIndex];
        if (displaced.span > dragged->span) {
            return std::nullopt;
        }
        displaced.slot = oldSlot;
    }

    if (!isValid(result, totalSlots)) {
        return std::nullopt;
    }
    return result;
}
}
