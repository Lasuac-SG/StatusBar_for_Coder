#include "core/layout_engine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

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

[[nodiscard]] bool canPlace(
    const std::vector<LayoutItem>& items,
    std::size_t itemIndex,
    int candidateSlot) noexcept
{
    const auto& item = items[itemIndex];
    for (std::size_t index = 0; index < itemIndex; ++index) {
        const auto& placed = items[index];
        if (overlaps(candidateSlot, item.span, placed.slot, placed.span)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool placeNormalized(
    std::vector<LayoutItem>& items,
    std::size_t itemIndex,
    int totalSlots)
{
    if (itemIndex == items.size()) {
        return true;
    }

    auto& item = items[itemIndex];
    const int originalSlot = item.slot;
    const int lastSlot = totalSlots - item.span;
    const int preferredSlot = std::clamp(originalSlot, 0, lastSlot);
    const auto maxDistance = std::max(
        static_cast<std::int64_t>(preferredSlot),
        static_cast<std::int64_t>(lastSlot) - preferredSlot);

    for (std::int64_t distance = 0; distance <= maxDistance; ++distance) {
        const auto lower = static_cast<std::int64_t>(preferredSlot) - distance;
        if (lower >= 0 && canPlace(items, itemIndex, static_cast<int>(lower))) {
            item.slot = static_cast<int>(lower);
            if (placeNormalized(items, itemIndex + 1, totalSlots)) {
                return true;
            }
        }

        const auto upper = static_cast<std::int64_t>(preferredSlot) + distance;
        if (distance != 0 && upper <= lastSlot
            && canPlace(items, itemIndex, static_cast<int>(upper))) {
            item.slot = static_cast<int>(upper);
            if (placeNormalized(items, itemIndex + 1, totalSlots)) {
                return true;
            }
        }
    }

    item.slot = originalSlot;
    return false;
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
    if (totalSlots < 0 || !hasValidIdentityAndSpan(items)) {
        return {};
    }

    std::int64_t requiredSlots = 0;
    for (const auto& item : items) {
        requiredSlots += item.span;
        if (item.span > totalSlots || requiredSlots > totalSlots) {
            return {};
        }
    }

    if (!placeNormalized(items, 0, totalSlots)) {
        return {};
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
