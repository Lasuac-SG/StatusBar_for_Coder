#pragma once

#include "core/widget_config.h"
#include "widgets/widget_view_model.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QTimer>
#include <QTimeZone>

#include <functional>

namespace Widgets {

using ClockNowProvider = std::function<QDateTime()>;

class ClockViewModel final : public WidgetViewModel {
    Q_OBJECT
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QJsonObject settings READ settings CONSTANT)
    Q_PROPERTY(QString timeText READ timeText NOTIFY timeTextChanged)

public:
    explicit ClockViewModel(
        Core::WidgetConfig config,
        ClockNowProvider nowProvider = {},
        QObject* parent = nullptr);

    [[nodiscard]] const QString& instanceId() const noexcept override { return m_config.id; }
    [[nodiscard]] const QJsonObject& settings() const noexcept { return m_config.settings; }
    [[nodiscard]] const QString& timeText() const noexcept { return m_timeText; }

    void Update();

signals:
    void timeTextChanged();

private:
    static constexpr qint64 minuteIntervalMs = 60000;
    static constexpr qint64 coarseTimerEarlyPercent = 5;
    static constexpr qint64 maximumLegitimateEarlyMs =
        minuteIntervalMs * coarseTimerEarlyPercent / 100;

    void onTimerTimeout();
    void updateTimeText(const QDateTime& now);
    void scheduleNextMinute(const QDateTime& now);
    void armTimer(qint64 intervalMs, Qt::TimerType timerType);

    Core::WidgetConfig m_config;
    ClockNowProvider m_nowProvider;
    QTimer m_timer;
    QTimeZone m_timeZone;
    QString m_format;
    QString m_timeText{QStringLiteral("--:--")};
    QDateTime m_nextMinuteTarget;
};

} // namespace Widgets
