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
        std::ifstream file("config.json");
        if (file.is_open()) {
            try {
                json j;
                file >> j; // 解析 JSON
                
                // 防御性编程：确保字段存在且类型为数组
                if (j.contains("active_widgets") && j["active_widgets"].is_array()) {
                    m_activeWidgets = j["active_widgets"].get<std::vector<std::string>>();
                }
            } catch (const std::exception& e) {
                std::cerr << "[Error] config.json 解析失败，将使用默认配置: " << e.what() << std::endl;
            }
        } else {
            // 文件不存在，自动生成一份标准配置
            std::cout << "[Info] 未发现 config.json，正在生成默认配置文件。" << std::endl;
            Save();
        }
    }

    void ConfigManager::Save() {
        json j;
        j["active_widgets"] = m_activeWidgets;
        
        std::ofstream file("config.json");
        if (file.is_open()) {
            // dump(4) 表示以 4 个空格的缩进进行美化输出
            file << j.dump(4);
        }
    }

    std::vector<std::string> ConfigManager::GetActiveWidgets() const {
        return m_activeWidgets;
    }
}
