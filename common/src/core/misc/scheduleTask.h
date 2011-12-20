#ifndef _vms_schedule_task_h
#define _vms_schedule_task_h

#include <QSharedPointer>
#include <QList>

#include "utils/common/qnid.h"

class QnScheduleTask
{
public:
    enum RecordingType { RecordingType_Run, RecordingType_MotionOnly, RecordingType_Never };

    QnScheduleTask()
        : m_startTime(0), m_endTime(0),
          m_doRecordAudio(false),
          m_recordType(RecordingType_Run),
          m_dayOfWeek(1),
          m_beforeThreshold(0), m_afterThreshold(0)
    {}
    QnScheduleTask(QnId id, QnId sourceId, int startTime, int endTime,
                   bool doRecordAudio, RecordingType recordType, int dayOfWeek, int beforeThreshold, int afterThreshold)
        : m_id(id), m_sourceId(sourceId),
          m_startTime(startTime), m_endTime(endTime),
          m_doRecordAudio(doRecordAudio),
          m_recordType(recordType),
          m_dayOfWeek(dayOfWeek),
          m_beforeThreshold(beforeThreshold), m_afterThreshold(afterThreshold)
    {}

    QnId getId() const { return m_id; }
    QnId getSourceId() const { return m_sourceId; }
    RecordingType getRecordingType() const { return m_recordType; }
    int getBeforeThreshold() const { return m_beforeThreshold; }
    int getAfterThreshold() const { return m_afterThreshold; }

    /*
    * Duration at ms
    */
    int durationMs() const;

    /*
    * Schedule relative start time inside a week at ms.
    */
    int startTimeMs() const;

    /*
    * Schedule relative start time inside a week at ms.
    */
    bool containTimeMs(int weekTimeMs) const;

    bool operator<(const QnScheduleTask &other) const;

    // Add getters and settings here

// private:
    QnId m_id;
    QnId m_sourceId;
    int m_startTime;
    int m_endTime;
    bool m_doRecordAudio;
    RecordingType m_recordType;
    int m_dayOfWeek;
    int m_beforeThreshold;
    int m_afterThreshold;
};

bool operator<(qint64 first, const QnScheduleTask &other);
bool operator<(const QnScheduleTask &other, qint64 first);

//typedef QSharedPointer<QnScheduleTask> QnScheduleTaskPtr;
typedef QList<QnScheduleTask> QnScheduleTaskList;

#endif // _vms_schedule_task_h
