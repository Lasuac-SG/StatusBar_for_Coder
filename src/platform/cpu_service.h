#pragma once

#include "platform/cpu_data_source.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

namespace Platform {

class CpuService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cpuPercent READ cpuPercent NOTIFY cpuPercentChanged)
    Q_PROPERTY(int currentFrequencyMHz READ currentFrequencyMHz NOTIFY currentFrequencyMHzChanged)
    Q_PROPERTY(int maxFrequencyMHz READ maxFrequencyMHz CONSTANT)
    Q_PROPERTY(int physicalCores READ physicalCores CONSTANT)
    Q_PROPERTY(int logicalCores READ logicalCores CONSTANT)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    explicit CpuService(std::unique_ptr<CpuDataSource> source, QObject* parent = nullptr);
    ~CpuService() override;

    [[nodiscard]] int cpuPercent() const noexcept { return m_cpuPercent; }
    [[nodiscard]] int currentFrequencyMHz() const noexcept { return m_currentFrequencyMHz; }
    [[nodiscard]] int maxFrequencyMHz() const noexcept { return m_maxFrequencyMHz; }
    [[nodiscard]] int physicalCores() const noexcept { return m_physicalCores; }
    [[nodiscard]] int logicalCores() const noexcept { return m_logicalCores; }
    [[nodiscard]] const QVariantList& history() const noexcept { return m_historyCache; }

    void start();
    void stop();
    Q_INVOKABLE void sampleNow();

signals:
    void cpuPercentChanged();
    void currentFrequencyMHzChanged();
    void historyChanged();

private:
    static constexpr std::size_t historyCapacity = 30;

    void appendHistory(int value);
    void updateFrequency(std::optional<double> ratio);

    std::unique_ptr<CpuDataSource> m_source;
    QTimer m_timer;
    std::optional<CpuTimes> m_previousTimes;
    std::array<int, historyCapacity> m_historyRing{};
    std::size_t m_historyNext{};
    std::size_t m_historyCount{};
    QVariantList m_historyCache;
    int m_cpuPercent{};
    int m_currentFrequencyMHz{};
    int m_maxFrequencyMHz{};
    int m_physicalCores{};
    int m_logicalCores{};
};

} // namespace Platform
