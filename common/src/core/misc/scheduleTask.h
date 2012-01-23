#ifndef _vms_schedule_task_h
#define _vms_schedule_task_h

#include <QSharedPointer>
#include <QList>

#include "core/resource/media_resource.h"
#include "utils/common/qnid.h"

class QnScheduleTask
{
public:
    enum RecordingType { RecordingType_Run, RecordingType_MotionOnly, RecordingType_Never };

    QnScheduleTask()
        : m_id(0), m_resourceId(0),
          m_startTime(0), m_endTime(0),
          m_doRecordAudio(false),
          m_recordType(RecordingType_Run),
          m_dayOfWeek(1),
          m_beforeThreshold(0), m_afterThreshold(0),
          m_streamQuality(QnQualityHighest),
          m_fps(10)
    {}
    QnScheduleTask(QnId id, QnId resourceId, int startTime, int endTime,
                   bool doRecordAudio, RecordingType recordType, int dayOfWeek, int beforeThreshold, int afterThreshold,
                   QnStreamQuality streamQuality = QnQualityHighest, int fps = 10)
        : m_id(id), m_resourceId(resourceId),
          m_startTime(startTime), m_endTime(endTime),
          m_doRecordAudio(doRecordAudio),
          m_recordType(recordType),
          m_dayOfWeek(dayOfWeek),
          m_beforeThreshold(beforeThreshold),
          m_afterThreshold(afterThreshold),
          m_streamQuality(streamQuality),
          m_fps(fps)
    {}

    bool operator<(const QnScheduleTask &other) const;

    QnId getId() const { return m_id; }
    QnId getResourceId() const { return m_resourceId; }
    RecordingType getRecordingType() const { return m_recordType; }
    int getBeforeThreshold() const { return m_beforeThreshold; }
    int getAfterThreshold() const { return m_afterThreshold; }
    int getFps() const { return m_fps; }
    QnStreamQuality getStreamQuality() const { return m_streamQuality; }
    bool getDoRecordAudio() const { return m_doRecordAudio; }
    int getDayOfWeek() const { return m_dayOfWeek; }
    int getStartTime() const { return m_startTime; }
    int getEndTime() const { return m_endTime; }

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

private:
    QnId m_id;
    QnId m_resourceId;
    int m_startTime;
    int m_endTime;
    bool m_doRecordAudio;
    RecordingType m_recordType;
    int m_dayOfWeek;
    int m_beforeThreshold;
    int m_afterThreshold;
    QnStreamQuality m_streamQuality;
    int m_fps;
};

bool operator<(qint64 first, const QnScheduleTask &other);
bool operator<(const QnScheduleTask &other, qint64 first);

//typedef QSharedPointer<QnScheduleTask> QnScheduleTaskPtr;
typedef QList<QnScheduleTask> QnScheduleTaskList;

#endif // _vms_schedule_task_h
