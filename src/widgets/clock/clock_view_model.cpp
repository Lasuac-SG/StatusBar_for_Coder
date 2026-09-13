#include "widgets/clock/clock_view_model.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Widgets {
namespace {

[[nodiscard]] QTimeZone configuredTimeZone(const QJsonObject& settings)
{
    QJsonValue value = settings.value(QStringLiteral("timeZone"));
    if (value.isUndefined()) {
        value = settings.value(QStringLiteral("timezone"));
    }
    if (value.isString()) {
        const QTimeZone configured(value.toString().toUtf8());
        if (configured.isValid()) {
            return configured;
        }
    }
    return QTimeZone::systemTimeZone();
}

[[nodiscard]] QString configuredFormat(const QJsonObject& settings)
{
    const QJsonValue value = settings.value(QStringLiteral("format"));
    if (value.isString()) {
        const QString format = value.toString().trimmed();
        if (!format.isEmpty() && format.size() <= 128) {
            return format;
        }
    }
    return QStringLiteral("HH:mm");
}

} // namespace

ClockViewModel::ClockViewModel(
    Core::WidgetConfig config,
    ClockNowProvider nowProvider,
    QObject* parent)
    : WidgetViewModel(parent)
    , m_config(std::move(config))
    , m_nowProvider(std::move(nowProvider))
    , m_timer(this)
    , m_timeZone(configuredTimeZone(m_config.settings))
    , m_format(configuredFormat(m_config.settings))
{
    if (!m_nowProvider) {
        m_nowProvider = [] { return QDateTime::currentDateTime(); };
    }
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &ClockViewModel::onTimerTimeout);
    Update();
}

void ClockViewModel::Update()
{
    const QDateTime now = m_nowProvider();
    if (!now.isValid()) {
        m_nextMinuteTarget = {};
        armTimer(minuteIntervalMs, Qt::CoarseTimer);
        return;
    }

    updateTimeText(now);
    scheduleNextMinute(now);
}

void ClockViewModel::onTimerTimeout()
{
    const QDateTime now = m_nowProvider();
    if (!now.isValid() || !m_nextMinuteTarget.isValid()) {
        Update();
        return;
    }

    const qint64 remaining = now.msecsTo(m_nextMinuteTarget);
    if (remaining > 0 && remaining <= maximumLegitimateEarlyMs) {
        armTimer(remaining, Qt::PreciseTimer);
        return;
    }

    updateTimeText(now);
    scheduleNextMinute(now);
}

void ClockViewModel::updateTimeText(const QDateTime& now)
{
    const QString text = now.toTimeZone(m_timeZone).toString(m_format);
    if (m_timeText != text) {
        m_timeText = text;
        emit timeTextChanged();
    }
}

void ClockViewModel::scheduleNextMinute(const QDateTime& now)
{
    const QTime time = now.time();
    const int elapsedInMinute = time.second() * 1000 + time.msec();
    const qint64 remaining =
        std::max(qint64{1}, minuteIntervalMs - elapsedInMinute);
    m_nextMinuteTarget = now.addMSecs(remaining);
    armTimer(
        remaining,
        remaining <= maximumLegitimateEarlyMs ? Qt::PreciseTimer : Qt::CoarseTimer);
}

void ClockViewModel::armTimer(const qint64 intervalMs, const Qt::TimerType timerType)
{
    const qint64 bounded = std::clamp(
        intervalMs,
        qint64{1},
        static_cast<qint64>(std::numeric_limits<int>::max()));
    m_timer.stop();
    m_timer.setTimerType(timerType);
    m_timer.start(static_cast<int>(bounded));
}

} // namespace Widgets
