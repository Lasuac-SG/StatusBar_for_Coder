#include "platform/cpu_service.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Platform {
namespace {

[[nodiscard]] std::uint64_t fileTimeValue(const FILETIME& value) noexcept
{
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U)
        | static_cast<std::uint64_t>(value.dwLowDateTime);
}

[[nodiscard]] bool validPdhStatus(const DWORD status) noexcept
{
    return status == PDH_CSTATUS_VALID_DATA || status == PDH_CSTATUS_NEW_DATA;
}

} // namespace

class WindowsCpuDataSource::Impl final {
public:
    Impl() noexcept
    {
        PDH_HQUERY openedQuery{};
        if (PdhOpenQueryW(nullptr, 0, &openedQuery) != ERROR_SUCCESS) {
            return;
        }
        query = openedQuery;

        if (PdhAddEnglishCounterW(
                query,
                L"\\Processor Information(_Total)\\% Processor Performance",
                0,
                &counter)
            != ERROR_SUCCESS) {
            closeQuery();
            return;
        }
        if (PdhCollectQueryData(query) != ERROR_SUCCESS) {
            closeQuery();
        }
    }

    ~Impl() { closeQuery(); }

    void closeQuery() noexcept
    {
        if (query != nullptr) {
            PdhCloseQuery(query);
            query = nullptr;
            counter = nullptr;
        }
    }

    PDH_HQUERY query{};
    PDH_HCOUNTER counter{};
};

WindowsCpuDataSource::WindowsCpuDataSource()
    : m_impl(std::make_unique<Impl>())
{
}

WindowsCpuDataSource::~WindowsCpuDataSource() = default;

std::optional<CpuTimes> WindowsCpuDataSource::sampleTimes() noexcept
{
    FILETIME idle{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return std::nullopt;
    }
    return CpuTimes{fileTimeValue(idle), fileTimeValue(kernel), fileTimeValue(user)};
}

std::optional<double> WindowsCpuDataSource::samplePerformanceRatio() noexcept
{
    if (m_impl == nullptr || m_impl->query == nullptr || m_impl->counter == nullptr) {
        return std::nullopt;
    }
    if (PdhCollectQueryData(m_impl->query) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    PDH_FMT_COUNTERVALUE value{};
    if (PdhGetFormattedCounterValue(
            m_impl->counter, PDH_FMT_DOUBLE, nullptr, &value)
            != ERROR_SUCCESS
        || !validPdhStatus(value.CStatus) || !std::isfinite(value.doubleValue)
        || value.doubleValue < 0.0) {
        return std::nullopt;
    }
    return value.doubleValue / 100.0;
}

std::optional<CpuTopology> WindowsCpuDataSource::topology() noexcept
{
    const DWORD logical = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (logical == 0) {
        return std::nullopt;
    }

    DWORD bytes{};
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0) {
        return std::nullopt;
    }

    try {
        std::vector<unsigned char> buffer(bytes);
        auto* const information =
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (!GetLogicalProcessorInformationEx(RelationProcessorCore, information, &bytes)) {
            return std::nullopt;
        }

        int physical{};
        DWORD offset{};
        while (offset < bytes) {
            if (bytes - offset < sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)) {
                return std::nullopt;
            }
            const auto* const entry =
                reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                    buffer.data() + offset);
            if (entry->Size == 0 || entry->Size > bytes - offset) {
                return std::nullopt;
            }
            if (entry->Relationship == RelationProcessorCore) {
                ++physical;
            }
            offset += entry->Size;
        }
        if (physical <= 0) {
            return std::nullopt;
        }

        DWORD frequency{};
        DWORD frequencyBytes = sizeof(frequency);
        const LSTATUS frequencyStatus = RegGetValueW(
            HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            L"~MHz",
            RRF_RT_REG_DWORD,
            nullptr,
            &frequency,
            &frequencyBytes);
        const int maxFrequency = frequencyStatus == ERROR_SUCCESS
            && frequency <= static_cast<DWORD>(std::numeric_limits<int>::max())
            ? static_cast<int>(frequency)
            : 0;
        const int logicalCount = logical <= static_cast<DWORD>(std::numeric_limits<int>::max())
            ? static_cast<int>(logical)
            : std::numeric_limits<int>::max();
        return CpuTopology{physical, logicalCount, maxFrequency};
    } catch (...) {
        return std::nullopt;
    }
}

CpuService::CpuService(std::unique_ptr<CpuDataSource> source, QObject* parent)
    : QObject(parent)
    , m_source(std::move(source))
    , m_timer(this)
{
    if (m_source != nullptr) {
        if (const auto value = m_source->topology()) {
            m_physicalCores = std::max(0, value->physicalCores);
            m_logicalCores = std::max(0, value->logicalCores);
            m_maxFrequencyMHz = std::max(0, value->maxFrequencyMHz);
        }
    }

    m_timer.setTimerType(Qt::CoarseTimer);
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &CpuService::sampleNow);
}

CpuService::~CpuService() = default;

void CpuService::start()
{
    if (m_source == nullptr || m_timer.isActive()) {
        return;
    }

    m_previousTimes.reset();
    sampleNow();
    m_timer.start();
}

void CpuService::stop()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    }
}

void CpuService::sampleNow()
{
    if (m_source == nullptr) {
        return;
    }

    const auto currentTimes = m_source->sampleTimes();
    if (currentTimes.has_value()) {
        if (!m_previousTimes.has_value()) {
            m_previousTimes = currentTimes;
        } else if (const auto usage = calculateCpuUsage(*m_previousTimes, *currentTimes)) {
            m_previousTimes = currentTimes;
            if (m_cpuPercent != *usage) {
                m_cpuPercent = *usage;
                emit cpuPercentChanged();
            }
            appendHistory(*usage);
        }
    }

    updateFrequency(m_source->samplePerformanceRatio());
}

void CpuService::appendHistory(const int value)
{
    m_historyRing[m_historyNext] = value;
    m_historyNext = (m_historyNext + 1) % historyCapacity;
    m_historyCount = std::min(m_historyCount + 1, historyCapacity);

    m_historyCache.clear();
    m_historyCache.reserve(static_cast<qsizetype>(m_historyCount));
    const std::size_t oldest = m_historyCount == historyCapacity ? m_historyNext : 0;
    for (std::size_t index = 0; index < m_historyCount; ++index) {
        m_historyCache.append(m_historyRing[(oldest + index) % historyCapacity]);
    }
    emit historyChanged();
}

void CpuService::updateFrequency(const std::optional<double> ratio)
{
    if (!ratio.has_value() || !std::isfinite(*ratio) || *ratio < 0.0
        || m_maxFrequencyMHz <= 0) {
        return;
    }

    const auto scaled = static_cast<long double>(m_maxFrequencyMHz)
        * static_cast<long double>(*ratio);
    const auto bounded = std::clamp(
        scaled, 0.0L, static_cast<long double>(std::numeric_limits<int>::max()));
    const int frequency = static_cast<int>(std::round(bounded));
    if (m_currentFrequencyMHz != frequency) {
        m_currentFrequencyMHz = frequency;
        emit currentFrequencyMHzChanged();
    }
}

} // namespace Platform
