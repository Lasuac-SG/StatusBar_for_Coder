#pragma once
#include "src/ui/viewmodels/i_view_model.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <iostream>

namespace UI {
    class WidgetRegistry {
    public:
        // 工厂函数变为无参，实现绝对解耦
        using FactoryFunc = std::function<std::unique_ptr<IViewModel>()>;

        static WidgetRegistry& GetInstance() {
            static WidgetRegistry instance;
            return instance;
        }

        void Register(const std::string& name, FactoryFunc factory) {
            m_factories[name] = std::move(factory);
        }

        std::unique_ptr<IViewModel> Create(const std::string& name) {
            if (m_factories.find(name) != m_factories.end()) {
                return m_factories[name]();
            }
            std::cerr << "[Warning] WidgetRegistry: 未找到名为 " << name << " 的组件！" << std::endl;
            return nullptr; 
        }

    private:
        WidgetRegistry() = default;
        ~WidgetRegistry() = default;
        WidgetRegistry(const WidgetRegistry&) = delete;
        WidgetRegistry& operator=(const WidgetRegistry&) = delete;

        std::unordered_map<std::string, FactoryFunc> m_factories;
    };
}
