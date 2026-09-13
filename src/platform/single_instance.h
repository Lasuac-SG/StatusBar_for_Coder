#pragma once

#include "core/result.h"

#include <windows.h>

namespace Platform {

class SingleInstance final {
public:
    SingleInstance() = default;
    ~SingleInstance() noexcept;

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;
    SingleInstance(SingleInstance&&) = delete;
    SingleInstance& operator=(SingleInstance&&) = delete;

    [[nodiscard]] Core::Result<void> initialize(const wchar_t* name);
    [[nodiscard]] bool alreadyRunning() const noexcept { return alreadyRunning_; }
    [[nodiscard]] bool ownsHandle() const noexcept { return handle_ != nullptr; }

private:
    HANDLE handle_{};
    bool alreadyRunning_{};
};

} // namespace Platform
