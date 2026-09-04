#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace Platform {

struct CpuTimes final {
    std::uint64_t idle;
    std::uint64_t kernel;
    std::uint64_t user;

    friend bool operator==(const CpuTimes&, const CpuTimes&) = default;
};

[[nodiscard]] inline std::optional<int> calculateCpuUsage(
    const CpuTimes previous,
    const CpuTimes current) noexcept
{
    if (current.idle < previous.idle || current.kernel < previous.kernel
        || current.user < previous.user) {
        return std::nullopt;
    }

    const auto idleDelta = current.idle - previous.idle;
    const auto kernelDelta = current.kernel - previous.kernel;
    const auto userDelta = current.user - previous.user;
    if (kernelDelta == 0 && userDelta == 0) {
        return std::nullopt;
    }

    const auto total = static_cast<long double>(kernelDelta)
        + static_cast<long double>(userDelta);
    const auto idleRatio = static_cast<long double>(idleDelta) / total;
    const auto busyRatio = std::clamp(1.0L - idleRatio, 0.0L, 1.0L);
    return static_cast<int>(busyRatio * 100.0L);
}

} // namespace Platform
