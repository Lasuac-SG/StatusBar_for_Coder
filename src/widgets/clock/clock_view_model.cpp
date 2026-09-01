#include "src/widgets/clock/clock_view_model.h"
#include "src/widgets/clock/clock_adapter.h"
#include <chrono>
#include <ctime>

namespace Widgets {
    ClockViewModel::ClockViewModel() {
        Update(); 
    }

    void ClockViewModel::Update() {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        struct tm localTime;
        localtime_s(&localTime, &currentTime);

        char timeBuffer[16];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &localTime);
        ClockAdapter::GetInstance().SetTimeText(QString::fromUtf8(timeBuffer));
    }
}
