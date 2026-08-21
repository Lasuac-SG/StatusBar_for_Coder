#pragma once
#include <QObject>
#include <QString>

namespace UI {
    class ClockAdapter : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString timeText READ timeText WRITE setTimeText NOTIFY timeTextChanged)
    public:
        static ClockAdapter& GetInstance() {
            static ClockAdapter instance;
            return instance;
        }
        QString timeText() const { return m_timeText; }
        void setTimeText(const QString& time) {
            if (m_timeText != time) {
                m_timeText = time;
                emit timeTextChanged();
            }
        }
    signals:
        void timeTextChanged();
    private:
        ClockAdapter() = default;
        QString m_timeText = "--:--";
    };
}
