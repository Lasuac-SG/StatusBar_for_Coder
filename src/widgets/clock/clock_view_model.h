#pragma once

#include "core/widget_config.h"
#include "widgets/i_widget_view_model.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QTimer>
#include <QTimeZone>

#include <functional>
#include <string>

namespace Widgets {

using ClockNowProvider = std::function<QDateTime()>;

class ClockViewModel final : public IWidgetViewModel {
    Q_OBJECT
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QJsonObject settings READ settings CONSTANT)
    Q_PROPERTY(QString timeText READ timeText NOTIFY timeTextChanged)

public:
    explicit ClockViewModel(
        Core::WidgetConfig config,
        ClockNowProvider nowProvider = {},
        QObject* parent = nullptr);

    [[nodiscard]] const QString& instanceId() const noexcept { return m_config.id; }
    [[nodiscard]] const QJsonObject& settings() const noexcept { return m_config.settings; }
    [[nodiscard]] const QString& timeText() const noexcept { return m_timeText; }

    [[nodiscard]] int GetSpan() const override { return 3; }
    [[nodiscard]] std::string GetKind() const override { return "Clock"; }
    void Update() override;

signals:
    void timeTextChanged();

private:
    static constexpr qint64 preciseWindowMs = 3000;

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
