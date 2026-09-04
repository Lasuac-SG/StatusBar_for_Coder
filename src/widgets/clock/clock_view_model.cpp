#include "widgets/clock/clock_view_model.h"

#include <algorithm>
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
    : IWidgetViewModel(parent)
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
    connect(&m_timer, &QTimer::timeout, this, &ClockViewModel::Update);
    Update();
}

void ClockViewModel::Update()
{
    const QDateTime now = m_nowProvider();
    if (!now.isValid()) {
        m_timer.start(60000);
        return;
    }

    const QString text = now.toTimeZone(m_timeZone).toString(m_format);
    if (m_timeText != text) {
        m_timeText = text;
        emit timeTextChanged();
    }
    scheduleNextMinute(now);
}

void ClockViewModel::scheduleNextMinute(const QDateTime& now)
{
    const QTime time = now.time();
    const int elapsedInMinute = time.second() * 1000 + time.msec();
    m_timer.start(std::max(1, 60000 - elapsedInMinute));
}

} // namespace Widgets
