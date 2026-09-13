#pragma once

#include "platform/cpu_data_source.h"

#include <QObject>
#include <QPointer>
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
    class ConsumerLease final {
    public:
        ConsumerLease() noexcept = default;
        ~ConsumerLease();

        ConsumerLease(const ConsumerLease&) = delete;
        ConsumerLease& operator=(const ConsumerLease&) = delete;
        ConsumerLease(ConsumerLease&& other) noexcept;
        ConsumerLease& operator=(ConsumerLease&& other) noexcept;

    private:
        friend class CpuService;

        explicit ConsumerLease(CpuService& service) noexcept;
        void reset() noexcept;

        QPointer<CpuService> m_service;
    };

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
    [[nodiscard]] ConsumerLease acquireConsumer();
    Q_INVOKABLE void sampleNow();

signals:
    void cpuPercentChanged();
    void currentFrequencyMHzChanged();
    void historyChanged();

private:
    static constexpr std::size_t historyCapacity = 30;

    void appendHistory(int value);
    void updateFrequency(std::optional<double> ratio);
    void updateRunningState();
    void releaseConsumer() noexcept;

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
    std::size_t m_consumerCount{};
    bool m_manualStartRequested{};
};

} // namespace Platform
