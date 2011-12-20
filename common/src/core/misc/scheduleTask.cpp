#include "scheduleTask.h"
#include "utils/common/util.h"

bool QnScheduleTask::containTimeMs(int weekTimeMs) const
{
    return qnBetween(weekTimeMs, startTimeMs(), startTimeMs()+durationMs());
}

int QnScheduleTask::durationMs() const
{
    return 1000 * (m_endTime - m_startTime);
}

int QnScheduleTask::startTimeMs() const
{
    return 1000 * ((m_dayOfWeek-1)*3600*24 + m_startTime);
}

bool QnScheduleTask::operator< (const QnScheduleTask& other) const
{
    if (m_id < other.m_id)
        return true;
    else if (m_id > other.m_id)
        return false;
    
    return startTimeMs() < other.startTimeMs();
}

bool operator < (qint64 first, const QnScheduleTask& other)
{
    return first < other.startTimeMs();
}

bool operator < (const QnScheduleTask& other, qint64 first)
{
    return other.startTimeMs() < first;
}
