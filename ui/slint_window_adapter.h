#pragma once
#include "src/core/i_window_adapter.h"
#include "main.h" // 只有适配器层允许包含 Slint 生成的头文件，将污染限制在这里
#include <windows.h>

namespace UI {
    // 继承抽象接口，实现 Slint 专属的调用逻辑
    class SlintWindowAdapter : public Core::IWindowAdapter {
    public:
        SlintWindowAdapter();
        ~SlintWindowAdapter() override = default;

        // override 关键字确保编译器检查该函数必须覆盖父类的虚函数
        void SetDpiScale(float scale) override;
        void SetSize(uint32_t width, uint32_t height) override;
        void SetPosition(int x, int y) override;
        void Show() override;
        void Run() override;
        
        void HideFromAltTabAndTaskbar() override;

    private:
        // 在这里持有 Slint 专属的组件句柄
        slint::ComponentHandle<MainWindow> m_uiHandle;
        HWND GetHwnd() const;
    };
}
