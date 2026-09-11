#define WIN32_LEAN_AND_MEAN
#include "platform/windows_logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

#include <windows.h>

#include <iostream>

namespace Platform {

namespace {

QMutex logMutex;

QString levelName(const WindowsLogLevel level)
{
    return level == WindowsLogLevel::Error
        ? QStringLiteral("ERROR")
        : QStringLiteral("WARNING");
}

} // namespace

void LogWindowsMessage(const WindowsLogLevel level, const QString& message) noexcept
{
    try {
        const QMutexLocker locker(&logMutex);
        const QString line = QStringLiteral("%1 [%2] %3\n")
                                 .arg(
                                     QDateTime::currentDateTimeUtc().toString(
                                         Qt::ISODateWithMs),
                                     levelName(level),
                                     message);

        OutputDebugStringW(reinterpret_cast<LPCWSTR>(line.utf16()));
        std::cerr << line.toStdString();

        const QString logDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (logDirectory.isEmpty() || !QDir().mkpath(logDirectory)) {
            return;
        }

        QFile file(QDir(logDirectory).filePath(QStringLiteral("statusbar.log")));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return;
        }
        const QByteArray bytes = line.toUtf8();
        if (file.write(bytes) == bytes.size()) {
            file.flush();
        }
    } catch (...) {
        OutputDebugStringW(L"StatusBar logging failed unexpectedly\n");
    }
}

} // namespace Platform
