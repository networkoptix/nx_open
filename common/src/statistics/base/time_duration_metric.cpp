
#include "time_duration_metric.h"

QnTimeDurationMetric::QnTimeDurationMetric(bool active)
    : m_counter()
    , m_isActiveState(false)
    , m_activeStateDurationMs(0)
{
    m_counter.invalidate();
    setCounterActive(active);
}

QnTimeDurationMetric::~QnTimeDurationMetric()
{}

void QnTimeDurationMetric::setCounterActive(bool isActive)
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

QString QnTimeDurationMetric::value() const
{
    return QString::number(duration());
}

bool QnTimeDurationMetric::isSignificant() const
{
    return (duration() > 0);
}

bool QnTimeDurationMetric::isCounterActive() const
{
    return m_isActiveState;
}

void QnTimeDurationMetric::reset()
{
    if (m_isActiveState)
        m_counter.restart();
    m_activeStateDurationMs = 0;
}

qint64 QnTimeDurationMetric::duration() const
{
    const qint64 countedMs = (m_counter.isValid()
        ? m_counter.elapsed() : 0);
    return m_activeStateDurationMs + countedMs;
}

