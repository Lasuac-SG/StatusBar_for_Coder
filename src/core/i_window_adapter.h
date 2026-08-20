#pragma once
#include <cstdint>

namespace Core {
    // IWindowAdapter 是一个纯虚类，作为前端 UI 和后端逻辑之间的契约协议。
    class IWindowAdapter {
    public:
        // 虚析构函数：确保子类被销毁时能正确调用其析构逻辑，防止内存泄漏。
        virtual ~IWindowAdapter() = default;

        // 设置前端 UI 的 DPI 缩放倍率。
        virtual void SetDpiScale(float scale) = 0;
        
        // 设置前端 UI 窗口的物理尺寸（宽、高）。
        virtual void SetSize(uint32_t width, uint32_t height) = 0;
        
        // 设置前端 UI 窗口在屏幕上的绝对物理坐标 (x, y)。
        virtual void SetPosition(int x, int y) = 0;
        
        // 显示窗口。
        virtual void Show() = 0;
        
        // 启动前端 UI 的事件循环。
        virtual void Run() = 0;

        virtual void HideFromAltTabAndTaskbar() = 0;

        virtual void Quit() = 0;
    };
}
