#include "widgets/cpu/cpu_adapter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <winreg.h>
#include <algorithm>
#include <cmath>

namespace Widgets {
    CpuAdapter& CpuAdapter::GetInstance() {
        static CpuAdapter instance;
        return instance;
    }

    CpuAdapter::CpuAdapter() {
        m_history.resize(MAX_HISTORY_POINTS, 0.0);
        InitHardwareTopology();
        InitPdhQuery();
        Update();
    }

    CpuAdapter::~CpuAdapter() {
        if (m_pdhQuery) {
            PdhCloseQuery(static_cast<PDH_HQUERY>(m_pdhQuery));
            m_pdhQuery = nullptr;
        }
    }

    void CpuAdapter::InitHardwareTopology() {
        // 1. 获取物理核心数
        DWORD length = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && length > 0) {
            std::vector<BYTE> buffer(length);
            if (GetLogicalProcessorInformationEx(RelationProcessorCore, 
                    reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &length)) {
                int count = 0;
                DWORD offset = 0;
                while (offset < length) {
                    auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
                    if (info->Relationship == RelationProcessorCore) {
                        count++;
                    }
                    offset += info->Size;
                }
                m_physicalCores = count;
            }
        }

        // 2. 获取逻辑核心数
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        m_logicalCores = static_cast<int>(sysInfo.dwNumberOfProcessors);
        if (m_physicalCores <= 0) m_physicalCores = m_logicalCores;

        // 3. 读取基准标称频率（Max / Base frequency，如 3900 MHz）
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD mhz = 0;
            DWORD size = sizeof(DWORD);
            if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS) {
                if (mhz > 0) {
                    m_maxFreq = static_cast<int>(mhz);
                }
            }
            RegCloseKey(hKey);
        }

        if (m_maxFreq <= 0) m_maxFreq = 3600;
        m_currentFreq = m_maxFreq;
    }

    void CpuAdapter::InitPdhQuery() {
        PDH_HQUERY query = nullptr;
        PDH_HCOUNTER counter = nullptr;

        if (PdhOpenQuery(nullptr, 0, &query) == ERROR_SUCCESS) {
            // 使用英文标准计数器名称，跨 Windows 语言版本均能正确识别
            if (PdhAddEnglishCounterW(query, L"\\Processor Information(_Total)\\% Processor Performance", 0, &counter) == ERROR_SUCCESS) {
                m_pdhQuery = query;
                m_pdhCounter = counter;
                PdhCollectQueryData(query); // 预采第一帧
            } else {
                PdhCloseQuery(query);
            }
        }
    }

    void CpuAdapter::QueryFrequencies() {
        if (!m_pdhQuery || !m_pdhCounter) return;

        PDH_HQUERY query = static_cast<PDH_HQUERY>(m_pdhQuery);
        PDH_HCOUNTER counter = static_cast<PDH_HCOUNTER>(m_pdhCounter);

        if (PdhCollectQueryData(query) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE val;
            if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
                // val.doubleValue 为实时性能比率（如 135% 睿频或 45% 节能）
                double ratio = val.doubleValue / 100.0;
                if (ratio > 0.05 && ratio < 3.0) {
                    m_currentFreq = static_cast<int>(std::round(m_maxFreq * ratio));
                    emit freqChanged();
                }
            }
        }
    }

    QVariantList CpuAdapter::GetHistory() const {
        QVariantList list;
        list.reserve(static_cast<int>(m_history.size()));
        for (qreal val : m_history) {
            list.append(val);
        }
        return list;
    }

    void CpuAdapter::Update() {
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            auto FileTimeToUint64 = [](const FILETIME& ft) {
                return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            };

            uint64_t idle = FileTimeToUint64(idleTime);
            uint64_t kernel = FileTimeToUint64(kernelTime);
            uint64_t user = FileTimeToUint64(userTime);

            if (m_prevIdleTime != 0) {
                uint64_t idleDiff = idle - m_prevIdleTime;
                uint64_t kernelDiff = kernel - m_prevKernelTime;
                uint64_t userDiff = user - m_prevUserTime;
                uint64_t totalDiff = kernelDiff + userDiff;

                if (totalDiff > 0) {
                    m_cpuPercent = static_cast<int>((1.0 - (static_cast<double>(idleDiff) / totalDiff)) * 100.0);
                    m_cpuPercent = std::clamp(m_cpuPercent, 0, 100);
                    emit cpuPercentChanged();
                }
            }

            m_prevIdleTime = idle;
            m_prevKernelTime = kernel;
            m_prevUserTime = user;
        }

        m_history.erase(m_history.begin());
        m_history.push_back(m_cpuPercent);
        emit historyChanged();

        QueryFrequencies();
    }
}
