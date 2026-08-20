#pragma once
#include <string>
#include <vector>

namespace Core {
    class ConfigManager {
    public:
        // 全局单例
        static ConfigManager& GetInstance();
        
        // 从磁盘加载配置
        void Load();
        
        // 序列化并保存到磁盘
        void Save();
        
        // 获取当前激活的组件列表
        std::vector<std::string> GetActiveWidgets() const;

    private:
        ConfigManager() = default;
        ~ConfigManager() = default;
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        // 默认配置（如果文件不存在或为空时生效）
        std::vector<std::string> m_activeWidgets = {"Clock", "Clock"}; 
    };
}
