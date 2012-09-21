#include "schedule_task.h"

#include "utils/common/util.h"

bool QnScheduleTask::containTimeMs(int weekTimeMs) const
{
    return qBetween(weekTimeMs, startTimeMs(), startTimeMs() + durationMs());
}

int QnScheduleTask::durationMs() const
{
    return 1000 * (m_data.m_endTime - m_data.m_startTime);
}

int QnScheduleTask::startTimeMs() const
{
    return 1000 * ((m_data.m_dayOfWeek - 1) * 3600 * 24 + m_data.m_startTime);
}
