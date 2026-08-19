#include "ui/slint_window_adapter.h"
#include <iostream>
#include <string>

namespace UI {
    SlintWindowAdapter::SlintWindowAdapter() 
        : m_uiHandle(MainWindow::create()) {
    }

    void SlintWindowAdapter::SetDpiScale(float scale) {
        m_uiHandle->set_dpi_scale(scale);
    }

    void SlintWindowAdapter::SetSize(uint32_t width, uint32_t height) {
        m_uiHandle->window().set_size(slint::PhysicalSize({ width, height }));
    }

    void SlintWindowAdapter::SetPosition(int x, int y) {
        m_uiHandle->window().set_position(slint::PhysicalPosition({ x, y }));
    }

    void SlintWindowAdapter::Show() {
        m_uiHandle->show();
    }

    void SlintWindowAdapter::Run() {
        m_uiHandle->run();
    }

    static BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam) {
        HWND* pRet = reinterpret_cast<HWND*>(lParam);
        
        if (GetParent(hwnd) == nullptr) {
            char className[256] = {0};
            GetClassNameA(hwnd, className, sizeof(className));
            std::string clsNameStr(className);

            // [核心修复1]：增加对 Winit 事件通信窗口的黑名单过滤
            if (clsNameStr != "GeekDashboardAppBarProxy" && 
                clsNameStr != "IME" && 
                clsNameStr != "MSCTFIME UI" &&
                clsNameStr != "Winit Thread Event Target") { 
                
                if (IsWindowVisible(hwnd)) {
                    *pRet = hwnd;
                    return FALSE; 
                }
            }
        }
        return TRUE; 
    }

    HWND SlintWindowAdapter::GetHwnd() const {
        HWND hwnd = nullptr;
        EnumThreadWindows(GetCurrentThreadId(), EnumThreadWndProc, reinterpret_cast<LPARAM>(&hwnd));
        return hwnd;
    }

    void SlintWindowAdapter::HideFromAltTabAndTaskbar() {
        // [核心修复2]：使用 invoke_from_event_loop 进行异步闭包执行
        // 将篡改逻辑丢入 Slint 的事件队列中。当这部分代码执行时，保证 windowManager 的 Run() 已经跑起来了，
        // Winit 的真实 UI 窗口也绝对创建完毕且完全可见了。
        slint::invoke_from_event_loop([this]() {
            HWND hwnd = this->GetHwnd();
            
            if (!hwnd) {
                std::cerr << "[Error] 未能捕获到有效的 Slint UI 窗口！" << std::endl;
                return;
            }

            char className[256] = {0};
            GetClassNameA(hwnd, className, sizeof(className));
            
            std::cout << "[Debug] --- 异步窗口属性篡改开始 ---" << std::endl;
            std::cout << "[Debug] 真正捕获到的 UI 窗口 HWND: " << hwnd << std::endl;
            std::cout << "[Debug] 窗口 Class: " << className << std::endl;

            LONG_PTR dwExStyle = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            
            // 剥离 Alt+Tab 和任务栏
            dwExStyle |= WS_EX_TOOLWINDOW;
            dwExStyle &= ~WS_EX_APPWINDOW;

            ::SetWindowLongPtr(hwnd, GWL_EXSTYLE, dwExStyle);
            
            std::cout << "[Debug] --- 异步窗口属性篡改结束 ---" << std::endl;

            // 立刻刷新缓冲区，确保样式应用
            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, 
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        });
    }
}
