#include "scheduleTask.h"

#include "utils/common/util.h"

bool QnScheduleTask::containTimeMs(int weekTimeMs) const
{
    return qnBetween(weekTimeMs, startTimeMs(), startTimeMs() + durationMs());
}

int QnScheduleTask::durationMs() const
{
    return 1000 * (m_endTime - m_startTime);
}

int QnScheduleTask::startTimeMs() const
{
    return 1000 * ((m_dayOfWeek - 1) * 3600 * 24 + m_startTime);
}
