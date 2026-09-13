#pragma once
#include "src/widgets/i_widget_view_model.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Widgets {
    class WidgetRegistry {
    public:
        using FactoryFunc = std::function<std::unique_ptr<IWidgetViewModel>()>;

        static WidgetRegistry& GetInstance() {
            static WidgetRegistry instance;
            return instance;
        }

        void Register(const std::string& name, FactoryFunc factory) {
            m_factories[name] = std::move(factory);
        }

        std::unique_ptr<IWidgetViewModel> Create(const std::string& name) const {
            auto it = m_factories.find(name);
            if (it != m_factories.end()) {
                return it->second();
            }
            return nullptr;
        }

    private:
        WidgetRegistry() = default;
        std::unordered_map<std::string, FactoryFunc> m_factories;
    };
}
