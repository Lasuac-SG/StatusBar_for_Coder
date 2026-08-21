#include "src/core/config_manager.h"
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
                
                // [架构升级]：解析包含槽位坐标的对象数组
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
                std::cerr << "[Error] JSON 解析失败，格式可能不兼容: " << e.what() << std::endl;
            }
        } 
        
        // 如果文件不存在或旧版格式解析失败，启用默认分布
        if (m_activeWidgets.empty()) {
            // 默认放两个时钟，分别在第 0 格和第 4 格
            m_activeWidgets = {{"Clock", 0}, {"Clock", 4}};
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
