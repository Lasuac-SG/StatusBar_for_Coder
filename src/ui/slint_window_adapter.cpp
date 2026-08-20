#include "src/ui/slint_window_adapter.h"
#include "src/ui/viewmodels/widget_registry.h"
#include "src/core/config_manager.h"
#include <string_view> // [新增] C++17 字符串视图
#include <vector>
#include <string>

namespace UI {
    SlintWindowAdapter::SlintWindowAdapter() 
        : m_uiHandle(MainWindow::create()) {
        
        // 1. 初始化并加载本地持久化配置
        auto& configMgr = Core::ConfigManager::GetInstance();
        configMgr.Load();
        std::vector<std::string> activeWidgets = configMgr.GetActiveWidgets();

        // 2. 将数组转换为 Slint 可识别的高性能内存模型，并注射给前端
        auto slintModel = std::make_shared<slint::VectorModel<slint::SharedString>>();
        for (const auto& w : activeWidgets) {
            slintModel->push_back(slint::SharedString(w));
        }
        m_uiHandle->global<LayoutConfig>().set_active_widgets(slintModel);

        // 3. 驱动注册表工厂，实例化对应数量的 ViewModel 供 C++ 后台逻辑使用
        auto& registry = WidgetRegistry::GetInstance();
        for (const auto& widgetName : activeWidgets) {
            if (auto widget = registry.Create(widgetName, m_uiHandle)) {
                m_viewModels.push_back(std::move(widget));
            }
        }

        // 4. 启动统一脉冲定时器
        m_updateTimer.start(slint::TimerMode::Repeated, std::chrono::seconds(1), [this]() {
            for (auto& vm : m_viewModels) {
                vm->Update();
            }
        });    
    }    
   

    void SlintWindowAdapter::SetDpiScale(float scale) { m_uiHandle->set_dpi_scale(scale); }
    void SlintWindowAdapter::SetSize(uint32_t width, uint32_t height) { m_uiHandle->window().set_size(slint::PhysicalSize({ width, height })); }
    void SlintWindowAdapter::SetPosition(int x, int y) { m_uiHandle->window().set_position(slint::PhysicalPosition({ x, y })); }
    void SlintWindowAdapter::Show() { m_uiHandle->show(); }
    void SlintWindowAdapter::Run() { m_uiHandle->run(); }
    void SlintWindowAdapter::Quit() { slint::quit_event_loop();}

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
