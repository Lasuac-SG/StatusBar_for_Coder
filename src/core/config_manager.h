#pragma once
#include <string>
#include <vector>

namespace Core {
    // [新增]：结构化的配置文件基元
    struct WidgetConfig {
        std::string name;
        int slot; // 记录组件停留在哪个网格上
    };

    class ConfigManager {
    public:
        static ConfigManager& GetInstance();
        void Load();
        void Save();
        
        std::vector<WidgetConfig> GetActiveWidgets() const;
        void SetActiveWidgets(const std::vector<WidgetConfig>& widgets);

    private:
        ConfigManager() = default;
        ~ConfigManager() = default;
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        std::vector<WidgetConfig> m_activeWidgets; 
    };
}
