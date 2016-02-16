
#include "time_duration_metric.h"

TimeDurationMetric::TimeDurationMetric()
    : m_counter()
    , m_isActiveState(false)
    , m_activeStateDurationMs(0)
{
    m_counter.invalidate();
}

TimeDurationMetric::~TimeDurationMetric()
{}

void TimeDurationMetric::activateCounter(bool isActive)
{
    if (isActive == m_isActiveState)
        return;

    m_isActiveState = isActive;
    if (m_isActiveState)
    {
        m_counter.start();
    }
    else
    {
        m_activeStateDurationMs = duration();
        m_counter.invalidate();
    }
}

QString TimeDurationMetric::value() const
{
    return QString::number(duration());
}

void TimeDurationMetric::reset()
{
    activateCounter(false);
    m_activeStateDurationMs = 0;
}

qint64 TimeDurationMetric::duration() const
{
    const qint64 countedMs = (m_counter.isValid()
        ? m_counter.elapsed() : 0);
    return m_activeStateDurationMs + countedMs;
}

