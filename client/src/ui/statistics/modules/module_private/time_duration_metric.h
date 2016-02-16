
#pragma once

#include <QtCore/QElapsedTimer>

class TimeDurationMetric
{
public:
    TimeDurationMetric(bool active = false);

    virtual ~TimeDurationMetric();

    void activateCounter(bool isActive);

    QString value() const;

    qint64 duration() const;

    void reset();

private:
    QElapsedTimer m_counter;
    bool m_isActiveState;
    qint64 m_activeStateDurationMs;
};