#pragma once
#include "src/ui/viewmodels/i_view_model.h"
#include "main.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <iostream>

namespace UI {
    class WidgetRegistry {
    public:
        // 定义工厂函数的函数签名：接受 UI 句柄，返回 IViewModel 的智能指针
        using FactoryFunc = std::function<std::unique_ptr<IViewModel>(const slint::ComponentHandle<MainWindow>&)>;

        // 获取全局单例 (C++11 线程安全的局部静态变量)
        static WidgetRegistry& GetInstance() {
            static WidgetRegistry instance;
            return instance;
        }

        // 注册一个组件的创建配方
        void Register(const std::string& name, FactoryFunc factory) {
            m_factories[name] = std::move(factory);
        }

        // 根据名称动态生产组件
        std::unique_ptr<IViewModel> Create(const std::string& name, const slint::ComponentHandle<MainWindow>& uiHandle) {
            if (m_factories.find(name) != m_factories.end()) {
                return m_factories[name](uiHandle);
            }
            std::cerr << "[Warning] WidgetRegistry: 未找到名为 " << name << " 的组件！" << std::endl;
            return nullptr; 
        }

    private:
        WidgetRegistry() = default;
        ~WidgetRegistry() = default;
        
        // 禁用拷贝和赋值，确保单例纯粹性
        WidgetRegistry(const WidgetRegistry&) = delete;
        WidgetRegistry& operator=(const WidgetRegistry&) = delete;

        std::unordered_map<std::string, FactoryFunc> m_factories;
    };
}
