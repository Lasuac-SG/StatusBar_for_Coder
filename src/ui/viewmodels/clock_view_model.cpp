#include "src/ui/viewmodels/clock_view_model.h"
#include <chrono>
#include "src/ui/viewmodels/clock_view_model.h"
#include <chrono>
#include <ctime>
#include <cstdio>

namespace UI {
    // 构造函数初始化列表直接对 m_uiHandle 进行赋值拷贝
    ClockViewModel::ClockViewModel(const slint::ComponentHandle<MainWindow>& uiHandle) 
        : m_uiHandle(uiHandle) {
        // 构造完成时立刻执行一次更新，避免初始启动时的 "--:--" 闪烁
        Update(); 
    }

    void ClockViewModel::Update() {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        
        struct tm localTime;
        localtime_s(&localTime, &currentTime);

        char timeBuffer[16];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);

        // 注入数据到前端
        m_uiHandle->global<ClockAdapter>().set_time_text(slint::SharedString(timeBuffer));
    }
}
