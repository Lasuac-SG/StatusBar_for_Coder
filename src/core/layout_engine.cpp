#include "core/layout_engine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>

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

struct FreeInterval final {
    std::int64_t start;
    std::int64_t end;
};

using FreeIntervals = std::vector<FreeInterval>;

[[nodiscard]] std::int64_t distanceFrom(std::int64_t candidate, std::int64_t original) noexcept
{
    return candidate >= original ? candidate - original : original - candidate;
}

[[nodiscard]] std::optional<std::int64_t> nearestCandidateFor(
    const FreeIntervals& freeIntervals,
    std::int64_t span,
    std::int64_t originalSlot)
{
    std::optional<std::int64_t> bestCandidate;
    for (const auto& interval : freeIntervals) {
        if (interval.end - interval.start < span) {
            continue;
        }

        const auto lastSlot = interval.end - span;
        const auto candidate = std::clamp(originalSlot, interval.start, lastSlot);
        if (!bestCandidate.has_value()) {
            bestCandidate = candidate;
            continue;
        }

        const auto candidateDistance = distanceFrom(candidate, originalSlot);
        const auto bestDistance = distanceFrom(*bestCandidate, originalSlot);
        if (candidateDistance < bestDistance
            || (candidateDistance == bestDistance && candidate < *bestCandidate)) {
            bestCandidate = candidate;
        }
    }
    return bestCandidate;
}

[[nodiscard]] FreeIntervals occupy(
    const FreeIntervals& freeIntervals,
    std::int64_t slot,
    std::int64_t span)
{
    FreeIntervals result;
    result.reserve(freeIntervals.size() + 1);
    bool occupied = false;
    for (const auto& interval : freeIntervals) {
        if (!occupied && interval.start <= slot && slot + span <= interval.end) {
            if (interval.start < slot) {
                result.push_back({interval.start, slot});
            }
            if (slot + span < interval.end) {
                result.push_back({slot + span, interval.end});
            }
            occupied = true;
            continue;
        }
        result.push_back(interval);
    }
    return result;
}

[[nodiscard]] std::optional<std::vector<LayoutItem>> greedyNearest(
    std::vector<LayoutItem> items,
    std::int64_t totalSlots)
{
    FreeIntervals freeIntervals{{0, totalSlots}};
    for (auto& item : items) {
        const auto candidate = nearestCandidateFor(freeIntervals, item.span, item.slot);
        if (!candidate.has_value() || *candidate < std::numeric_limits<int>::min()
            || *candidate > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }

        item.slot = static_cast<int>(*candidate);
        freeIntervals = occupy(freeIntervals, *candidate, item.span);
    }
    return items;
}

[[nodiscard]] std::vector<LayoutItem> spatialCompactionFallback(std::vector<LayoutItem> items)
{
    std::vector<std::size_t> spatialOrder(items.size());
    std::iota(spatialOrder.begin(), spatialOrder.end(), std::size_t{0});
    std::stable_sort(spatialOrder.begin(), spatialOrder.end(), [&items](auto left, auto right) {
        return items[left].slot < items[right].slot;
    });

    std::int64_t nextSlot = 0;
    for (const auto itemIndex : spatialOrder) {
        if (nextSlot < std::numeric_limits<int>::min()
            || nextSlot > std::numeric_limits<int>::max()) {
            return {};
        }
        items[itemIndex].slot = static_cast<int>(nextSlot);
        nextSlot += items[itemIndex].span;
    }
    return items;
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
    if (isValid(items, totalSlots)) {
        return items;
    }
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

    // Phase 1: greedy-nearest placement in stable input order.
    if (auto greedyResult = greedyNearest(items, availableSlots); greedyResult.has_value()) {
        return *greedyResult;
    }

    // Phase 2: spatial-compaction fallback by original slot, then input index.
    return spatialCompactionFallback(items);
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
