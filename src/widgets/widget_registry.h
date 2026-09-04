#pragma once
#include "core/widget_config.h"
#include "widgets/i_widget_view_model.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Platform {
class CpuService;
}

namespace Widgets {
    class WidgetRegistry {
    public:
        using FactoryFunc = std::function<std::unique_ptr<IWidgetViewModel>(
            const Core::WidgetConfig&, Platform::CpuService&)>;

        static WidgetRegistry& GetInstance() {
            static WidgetRegistry instance;
            return instance;
        }

        void Register(const std::string& name, FactoryFunc factory) {
            m_factories[name] = std::move(factory);
        }

        std::unique_ptr<IWidgetViewModel> Create(
            const Core::WidgetConfig& config,
            Platform::CpuService& cpuService) const {
            auto it = m_factories.find(config.type.toStdString());
            if (it != m_factories.end()) {
                return it->second(config, cpuService);
            }
            return nullptr;
        }

    private:
        WidgetRegistry() = default;
        std::unordered_map<std::string, FactoryFunc> m_factories;
    };
}
