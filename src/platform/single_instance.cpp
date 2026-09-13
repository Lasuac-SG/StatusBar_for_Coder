#include "platform/single_instance.h"

#include "platform/windows_logging.h"

namespace Platform {

SingleInstance::~SingleInstance() noexcept
{
    try {
        if (handle_ != nullptr && !CloseHandle(handle_)) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot close the single-instance mutex (Win32 error %1)")
                    .arg(GetLastError()));
        }
    } catch (...) {
        LogWindowsMessage(
            WindowsLogLevel::Error,
            QStringLiteral("Unknown failure while closing the single-instance mutex"));
    }
}

Core::Result<void> SingleInstance::initialize(const wchar_t* const name)
{
    if (handle_ != nullptr) {
        return Core::Result<void>::success();
    }
    if (name == nullptr || *name == L'\0') {
        return Core::Result<void>::failure(
            QStringLiteral("Single-instance mutex name must not be empty"));
    }

    SetLastError(ERROR_SUCCESS);
    const HANDLE handle = CreateMutexW(nullptr, FALSE, name);
    const DWORD status = GetLastError();
    if (handle == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot create the single-instance mutex (Win32 error %1)")
                .arg(status));
    }
    if (status != ERROR_SUCCESS && status != ERROR_ALREADY_EXISTS) {
        if (!CloseHandle(handle)) {
            LogWindowsMessage(
                WindowsLogLevel::Error,
                QStringLiteral("Cannot roll back the single-instance mutex (Win32 error %1)")
                    .arg(GetLastError()));
        }
        return Core::Result<void>::failure(
            QStringLiteral("Single-instance mutex returned unexpected Win32 status %1")
                .arg(status));
    }

    handle_ = handle;
    alreadyRunning_ = status == ERROR_ALREADY_EXISTS;
    return Core::Result<void>::success();
}

} // namespace Platform
