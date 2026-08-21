#include "src/ui/viewmodels/clock_view_model.h"
#include "src/ui/clock_adapter.h" // 引入上一步新建的 Qt Adapter
#include <chrono>
#include <ctime>

namespace UI {
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

        // 通过 Qt 单例适配器发送数据，彻底与 UI 窗口解耦
        ClockAdapter::GetInstance().setTimeText(QString(timeBuffer));
    }
}
