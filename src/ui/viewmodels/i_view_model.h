#pragma once
#include <string>

namespace UI {
    class IViewModel {
    public:
        virtual ~IViewModel() = default;
        virtual void Update() = 0; 
        virtual int GetSpan() const = 0; 
        
        virtual std::string GetName() const = 0; 
    };
}
