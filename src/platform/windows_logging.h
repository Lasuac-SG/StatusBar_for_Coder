#pragma once

#include <QString>

namespace Platform {

enum class WindowsLogLevel {
    Warning,
    Error,
};

void LogWindowsMessage(WindowsLogLevel level, const QString& message) noexcept;

} // namespace Platform
