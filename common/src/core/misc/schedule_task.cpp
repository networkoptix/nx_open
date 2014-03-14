#include "schedule_task.h"

#include <utils/math/math.h>

bool QnScheduleTask::containTimeMs(int weekTimeMs) const
{
    return qBetween(startTimeMs(), weekTimeMs, startTimeMs() + durationMs());
}

int QnScheduleTask::durationMs() const
{
    return 1000 * (m_data.m_endTime - m_data.m_startTime);
}

int QnScheduleTask::startTimeMs() const
{
    return 1000 * ((m_data.m_dayOfWeek - 1) * 3600 * 24 + m_data.m_startTime);
}

int QnScheduleTask::getHour() const 
{ 
    Q_ASSERT(m_data.m_startTime < 24*3600);
    return m_data.m_startTime/3600; 
}

int QnScheduleTask::getMinute() const 
{ 
    Q_ASSERT(m_data.m_startTime < 24*3600);
    return (m_data.m_startTime - getHour()*3600) / 60; 
}

int QnScheduleTask::getSecond() const 
{ 
    Q_ASSERT(m_data.m_startTime < 24*3600);
    return m_data.m_startTime % 60; 
}
