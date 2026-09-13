#pragma once
#include <QObject>
#include <QVariantList>
#include <vector>

namespace Widgets {
    class CpuAdapter : public QObject {
        Q_OBJECT
        Q_PROPERTY(int cpuPercent READ GetCpuPercent NOTIFY cpuPercentChanged)
        Q_PROPERTY(int currentFreq READ GetCurrentFreq NOTIFY freqChanged)
        Q_PROPERTY(int maxFreq READ GetMaxFreq NOTIFY freqChanged)
        Q_PROPERTY(int physicalCores READ GetPhysicalCores CONSTANT)
        Q_PROPERTY(int logicalCores READ GetLogicalCores CONSTANT)
        Q_PROPERTY(QVariantList history READ GetHistory NOTIFY historyChanged)

    public:
        static CpuAdapter& GetInstance();

        int GetCpuPercent() const { return m_cpuPercent; }
        int GetCurrentFreq() const { return m_currentFreq; }
        int GetMaxFreq() const { return m_maxFreq; }
        int GetPhysicalCores() const { return m_physicalCores; }
        int GetLogicalCores() const { return m_logicalCores; }
        QVariantList GetHistory() const;

        void Update();

    signals:
        void cpuPercentChanged();
        void freqChanged();
        void historyChanged();

    private:
        CpuAdapter();
        ~CpuAdapter() override;

        void InitHardwareTopology();
        void InitPdhQuery();
        void QueryFrequencies();

        int m_cpuPercent = 0;
        int m_currentFreq = 0;
        int m_maxFreq = 0;
        int m_physicalCores = 0;
        int m_logicalCores = 0;

        uint64_t m_prevIdleTime = 0;
        uint64_t m_prevKernelTime = 0;
        uint64_t m_prevUserTime = 0;

        // PDH 性能计数器句柄
        void* m_pdhQuery = nullptr;
        void* m_pdhCounter = nullptr;

        static constexpr size_t MAX_HISTORY_POINTS = 30;
        std::vector<qreal> m_history;
    };
}
