#pragma once

namespace Core {
    class IWindowAdapter {
    public:
        virtual ~IWindowAdapter() = default;
        virtual void Run() = 0;
        virtual void Quit() = 0;
    };
}
