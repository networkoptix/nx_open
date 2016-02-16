
#include "time_duration_metric.h"

TimeDurationMetric::TimeDurationMetric(bool active)
    : m_counter()
    , m_isActiveState(false)
    , m_activeStateDurationMs(0)
{
    m_counter.invalidate();
    activateCounter(active);
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

bool TimeDurationMetric::isActive() const
{
    return m_isActiveState;
}

void TimeDurationMetric::reset()
{
    if (m_isActiveState)
        m_counter.restart();
    m_activeStateDurationMs = 0;
}

qint64 TimeDurationMetric::duration() const
{
    const qint64 countedMs = (m_counter.isValid()
        ? m_counter.elapsed() : 0);
    return m_activeStateDurationMs + countedMs;
}

