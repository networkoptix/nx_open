
#pragma once

#include <QtCore/QElapsedTimer>

class ActiveTimeMetric
{
public:
    ActiveTimeMetric();

    virtual ~ActiveTimeMetric();

    void activateCounter(bool isActive);

    QString value() const;

    void reset();

private:
    qint64 duration() const;

private:
    QElapsedTimer m_counter;
    bool m_isActiveState;
    qint64 m_activeStateDurationMs;
};