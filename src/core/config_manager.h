#pragma once
#include <string>
#include <vector>

namespace Core {
    struct WidgetConfig {
        std::string name;
        int slot;
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
