#ifndef _vms_schedule_task_h
#define _vms_schedule_task_h

#include <QSharedPointer>
#include <QList>

#include "core/resource/media_resource.h"
#include "utils/common/qnid.h"
#include "core/dataprovider/media_streamdataprovider.h"

class QnScheduleTask
{
public:
    enum RecordingType { RecordingType_Run, RecordingType_MotionOnly, RecordingType_Never };

    QnScheduleTask()
        : m_id(0), m_resourceId(0),
          m_dayOfWeek(1),
          m_startTime(0), m_endTime(0),
          m_recordType(RecordingType_Run),
          m_beforeThreshold(0), m_afterThreshold(0),
          m_streamQuality(QnQualityHighest),
          m_fps(10),
          m_doRecordAudio(false)
    {}
    QnScheduleTask(QnId id, QnId resourceId, int dayOfWeek, int startTime, int endTime,
                   RecordingType recordType, int beforeThreshold, int afterThreshold,
                   QnStreamQuality streamQuality = QnQualityHighest, int fps = 10, bool doRecordAudio = false)
        : m_id(id), m_resourceId(resourceId),
          m_dayOfWeek(dayOfWeek),
          m_startTime(startTime), m_endTime(endTime),
          m_recordType(recordType),
          m_beforeThreshold(beforeThreshold),
          m_afterThreshold(afterThreshold),
          m_streamQuality(streamQuality),
          m_fps(fps),
          m_doRecordAudio(doRecordAudio)
    {}

    bool operator<(const QnScheduleTask &other) const;

    QnId getId() const { return m_id; }
    QnId getResourceId() const { return m_resourceId; }
    int getDayOfWeek() const { return m_dayOfWeek; }
    int getStartTime() const { return m_startTime; }
    int getEndTime() const { return m_endTime; }
    RecordingType getRecordingType() const { return m_recordType; }
    int getBeforeThreshold() const { return m_beforeThreshold; }
    int getAfterThreshold() const { return m_afterThreshold; }
    QnStreamQuality getStreamQuality() const { return m_streamQuality; }
    int getFps() const { return m_fps; }
    bool getDoRecordAudio() const { return m_doRecordAudio; }

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
    int m_dayOfWeek;
    int m_startTime;
    int m_endTime;
    RecordingType m_recordType;
    int m_beforeThreshold;
    int m_afterThreshold;
    QnStreamQuality m_streamQuality;
    int m_fps;
    bool m_doRecordAudio;

    friend class CameraScheduleWidget; // ###
};

inline bool operator<(qint64 first, const QnScheduleTask &other)
{ return first < other.startTimeMs(); }
inline bool operator<(const QnScheduleTask &other, qint64 first)
{ return other.startTimeMs() < first; }

//typedef QSharedPointer<QnScheduleTask> QnScheduleTaskPtr;
typedef QList<QnScheduleTask> QnScheduleTaskList;

Q_DECLARE_TYPEINFO(QnScheduleTask, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnScheduleTask)
Q_DECLARE_METATYPE(QnScheduleTaskList)

#endif // _vms_schedule_task_h
