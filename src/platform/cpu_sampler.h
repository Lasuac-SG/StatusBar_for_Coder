#pragma once
#include <windows.h>
#include <cstdint>
#include <algorithm>

namespace Platform {
    class CpuSampler {
    public:
        CpuSampler() {
            FILETIME idleTime, kernelTime, userTime;
            if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
                m_prevIdle = FileTimeToUint64(idleTime);
                m_prevKernel = FileTimeToUint64(kernelTime);
                m_prevUser = FileTimeToUint64(userTime);
            }
        }

        int SampleUsage() {
            FILETIME idleTime, kernelTime, userTime;
            if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
                return 0;
            }

            uint64_t idle = FileTimeToUint64(idleTime);
            uint64_t kernel = FileTimeToUint64(kernelTime);
            uint64_t user = FileTimeToUint64(userTime);

            uint64_t diffIdle = idle - m_prevIdle;
            uint64_t diffKernel = kernel - m_prevKernel;
            uint64_t diffUser = user - m_prevUser;

            m_prevIdle = idle;
            m_prevKernel = kernel;
            m_prevUser = user;

            uint64_t totalSystem = diffKernel + diffUser;
            if (totalSystem == 0) return 0;

            int usage = static_cast<int>((totalSystem - diffIdle) * 100 / totalSystem);
            return std::clamp(usage, 0, 100);
        }

    private:
        static uint64_t FileTimeToUint64(const FILETIME& ft) {
            return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        }

        uint64_t m_prevIdle = 0;
        uint64_t m_prevKernel = 0;
        uint64_t m_prevUser = 0;
    };
}
