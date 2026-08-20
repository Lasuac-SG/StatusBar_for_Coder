#pragma once

namespace UI {
    class IViewModel {
    public:
        virtual ~IViewModel() = default;
        
        // 只有 Update 被保留为契约
        virtual void Update() = 0; 
    };
}
