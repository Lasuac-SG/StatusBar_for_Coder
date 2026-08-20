#include "ui/slint_window_adapter.h"
#include <iostream>
#include <string_view> // [新增] C++17 字符串视图

namespace UI {
    SlintWindowAdapter::SlintWindowAdapter() : m_uiHandle(MainWindow::create()) {}
    void SlintWindowAdapter::SetDpiScale(float scale) { m_uiHandle->set_dpi_scale(scale); }
    void SlintWindowAdapter::SetSize(uint32_t width, uint32_t height) { m_uiHandle->window().set_size(slint::PhysicalSize({ width, height })); }
    void SlintWindowAdapter::SetPosition(int x, int y) { m_uiHandle->window().set_position(slint::PhysicalPosition({ x, y })); }
    void SlintWindowAdapter::Show() { m_uiHandle->show(); }
    void SlintWindowAdapter::Run() { m_uiHandle->run(); }

    static BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam) {
        HWND* pRet = reinterpret_cast<HWND*>(lParam);
        
        if (GetParent(hwnd) == nullptr) {
            char className[256] = {0};
            GetClassNameA(hwnd, className, sizeof(className));
            
            // [性能优化]：使用 std::string_view，它只是一个指针+长度的包装，
            // 没有任何堆内存分配，极速匹配！
            std::string_view clsNameView(className);

            if (clsNameView != "GeekDashboardAppBarProxy" && 
                clsNameView != "IME" && 
                clsNameView != "MSCTFIME UI" &&
                clsNameView != "Winit Thread Event Target") { 
                
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
        slint::invoke_from_event_loop([this]() {
            HWND hwnd = this->GetHwnd();
            if (!hwnd) return; // 生产环境去掉沉重的 cout 报错，直接防御性返回

            LONG_PTR dwExStyle = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            
            dwExStyle |= WS_EX_TOOLWINDOW;
            dwExStyle &= ~WS_EX_APPWINDOW;

            ::SetWindowLongPtr(hwnd, GWL_EXSTYLE, dwExStyle);
            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, 
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        });
    }
}
