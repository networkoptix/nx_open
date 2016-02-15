
#include "active_time_metric.h"

ActiveTimeMetric::ActiveTimeMetric()
    : m_counter()
    , m_isActiveState(false)
    , m_activeStateDurationMs(0)
{
}

ActiveTimeMetric::~ActiveTimeMetric()
{}

void ActiveTimeMetric::activateCounter(bool isActive)
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

QString ActiveTimeMetric::value() const
{
    return QString::number(duration());
}

void ActiveTimeMetric::reset()
{
    activateCounter(false);
    m_activeStateDurationMs = 0;
}

qint64 ActiveTimeMetric::duration() const
{
    const qint64 countedMs = (m_counter.isValid()
        ? m_counter.elapsed() : 0);
    return m_activeStateDurationMs + countedMs;
}

