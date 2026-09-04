#pragma once

#include "platform/cpu_usage.h"

#include <memory>
#include <optional>

namespace Platform {

struct CpuTopology final {
    int physicalCores{};
    int logicalCores{};
    int maxFrequencyMHz{};

    friend bool operator==(const CpuTopology&, const CpuTopology&) = default;
};

class CpuDataSource {
public:
    CpuDataSource() = default;
    virtual ~CpuDataSource() = default;

    CpuDataSource(const CpuDataSource&) = delete;
    CpuDataSource& operator=(const CpuDataSource&) = delete;
    CpuDataSource(CpuDataSource&&) = delete;
    CpuDataSource& operator=(CpuDataSource&&) = delete;

    [[nodiscard]] virtual std::optional<CpuTimes> sampleTimes() noexcept = 0;
    [[nodiscard]] virtual std::optional<double> samplePerformanceRatio() noexcept = 0;
    [[nodiscard]] virtual std::optional<CpuTopology> topology() noexcept = 0;
};

class WindowsCpuDataSource final : public CpuDataSource {
public:
    WindowsCpuDataSource();
    ~WindowsCpuDataSource() override;

    [[nodiscard]] std::optional<CpuTimes> sampleTimes() noexcept override;
    [[nodiscard]] std::optional<double> samplePerformanceRatio() noexcept override;
    [[nodiscard]] std::optional<CpuTopology> topology() noexcept override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Platform
