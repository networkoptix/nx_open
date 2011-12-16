#ifndef _vms_schedule_task_h
#define _vms_schedule_task_h

#include <QSharedPointer>
#include <QList>

#include "utils/common/qnid.h"

class QnScheduleTask
{
public:
    QnScheduleTask() {}

    QnScheduleTask(QnId id, QnId sourceId, int startTime, int endTime,
                   bool doRecordAudio, QString recordType, int dayOfWeek, int beforeThreshold, int afterThreshold)
      : m_id(id),
        m_sourceId(sourceId),
        m_startTime(startTime),
        m_endTime(endTime),
        m_doRecordAudio(doRecordAudio),
        m_recordType(recordType),
        m_dayOfWeek(dayOfWeek),
        m_beforeThreshold(beforeThreshold),
        m_afterThreshold(afterThreshold)
    {
    }

    // Add getters and settings here

// private:
    QnId m_id;
    QnId m_sourceId;
    int m_startTime;
    int m_endTime;
    bool m_doRecordAudio;
    QString m_recordType;
    int m_dayOfWeek;
    int m_beforeThreshold;
    int m_afterThreshold;
};

typedef QSharedPointer<QnScheduleTask> QnScheduleTaskPtr;
typedef QList<QnScheduleTaskPtr> QnScheduleTaskList;

#endif // _vms_schedule_task_h
