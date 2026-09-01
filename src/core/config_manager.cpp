#include "core/config_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Core {
    ConfigManager& ConfigManager::GetInstance() {
        static ConfigManager instance;
        return instance;
    }

    void ConfigManager::Load() {
        m_activeWidgets.clear();
        std::ifstream file("config.json");
        
        if (file.is_open()) {
            try {
                json j;
                file >> j; 
                if (j.contains("widgets") && j["widgets"].is_array()) {
                    for (const auto& item : j["widgets"]) {
                        if (item.contains("name") && item.contains("slot")) {
                            m_activeWidgets.push_back({
                                item["name"].get<std::string>(),
                                item["slot"].get<int>()
                            });
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[ConfigManager] JSON 解析失败: " << e.what() << std::endl;
            }
        } 
        
        if (m_activeWidgets.empty()) {
            m_activeWidgets = {{"Clock", 0}, {"Cpu", 4}};
            Save();
        }
    }

    void ConfigManager::Save() {
        json j;
        j["widgets"] = json::array();
        for (const auto& w : m_activeWidgets) {
            j["widgets"].push_back({{"name", w.name}, {"slot", w.slot}});
        }
        std::ofstream file("config.json");
        if (file.is_open()) {
            file << j.dump(4);
        }
    }

    std::vector<WidgetConfig> ConfigManager::GetActiveWidgets() const {
        return m_activeWidgets;
    }

    void ConfigManager::SetActiveWidgets(const std::vector<WidgetConfig>& widgets) {
        m_activeWidgets = widgets;
        Save();
    }
}
