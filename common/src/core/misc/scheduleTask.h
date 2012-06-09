#ifndef _vms_schedule_task_h
#define _vms_schedule_task_h

#include <QSharedPointer>
#include <QList>
#include <QTextStream>

#include "core/resource/media_resource.h"
#include "utils/common/qnid.h"
#include "core/dataprovider/media_streamdataprovider.h"

class QnScheduleTask
{
public:
    enum RecordingType { RecordingType_Run, RecordingType_MotionOnly, RecordingType_Never, RecordingType_MotionPlusLQ };

    struct Data
    {
        Data(int dayOfWeek = 1, int startTime = 0, int endTime = 0, RecordingType recordType = RecordingType_Never,
             int beforeThreshold = 0, int afterThreshold = 0, QnStreamQuality streamQuality = QnQualityHighest, int fps = 10, bool doRecordAudio = false)
            : m_dayOfWeek(dayOfWeek),
              m_startTime(startTime),
              m_endTime(endTime),
              m_recordType(recordType),
              m_beforeThreshold(beforeThreshold),
              m_afterThreshold(afterThreshold),
              m_streamQuality(streamQuality),
              m_fps(fps),
              m_doRecordAudio(doRecordAudio)
        {
        }

        int m_dayOfWeek;
        int m_startTime;
        int m_endTime;
        RecordingType m_recordType;
        int m_beforeThreshold;
        int m_afterThreshold;
        QnStreamQuality m_streamQuality;
        int m_fps;
        bool m_doRecordAudio;
    };

    QnScheduleTask()
        : m_id(0), m_resourceId(0)
    {}
    QnScheduleTask(const Data& data)
        : m_id(0), m_resourceId(0), m_data(data)
    {
    }

    QnScheduleTask(QnId id, QnId resourceId, int dayOfWeek, int startTime, int endTime,
                   RecordingType recordType, int beforeThreshold, int afterThreshold,
                   QnStreamQuality streamQuality = QnQualityHighest, int fps = 10, bool doRecordAudio = false)
        : m_id(id), m_resourceId(resourceId),
          m_data(dayOfWeek, startTime, endTime, recordType, beforeThreshold, afterThreshold, streamQuality, fps, doRecordAudio)
    {}

    const Data& getData() const { return m_data; }
    void setData(const Data& data) { m_data = data; }

    QnId getId() const { return m_id; }
    QnId getResourceId() const { return m_resourceId; }
    int getDayOfWeek() const { return m_data.m_dayOfWeek; }
    int getStartTime() const { return m_data.m_startTime; }
    int getEndTime() const { return m_data.m_endTime; }
    RecordingType getRecordingType() const { return m_data.m_recordType; }
    int getBeforeThreshold() const { return m_data.m_beforeThreshold; }
    int getAfterThreshold() const { return m_data.m_afterThreshold; }
    QnStreamQuality getStreamQuality() const { return m_data.m_streamQuality; }
    int getFps() const { return m_data.m_fps; }
    bool getDoRecordAudio() const { return m_data.m_doRecordAudio; }

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

    Data m_data;

    friend class QnCameraScheduleWidget; // ###
};

inline bool operator<(qint64 first, const QnScheduleTask &other)
{ return first < other.startTimeMs(); }
inline bool operator<(const QnScheduleTask &other, qint64 first)
{ return other.startTimeMs() < first; }

inline bool operator==(const QnScheduleTask::Data &e1, const QnScheduleTask::Data &e2)
{
    return e1.m_dayOfWeek == e2.m_dayOfWeek &&
        e1.m_startTime == e2.m_startTime &&
        e1.m_endTime == e2.m_endTime &&
        e1.m_recordType == e2.m_recordType &&
        e1.m_beforeThreshold == e2.m_beforeThreshold &&
        e1.m_afterThreshold == e2.m_afterThreshold &&
        e1.m_streamQuality == e2.m_streamQuality &&
        e1.m_fps == e2.m_fps &&
        e1.m_doRecordAudio == e2.m_doRecordAudio;
}

inline uint qHash(const QnScheduleTask::Data &key)
{
    return qHash(key.m_dayOfWeek ^ key.m_startTime ^ key.m_endTime);
}

inline QTextStream& operator<<(QTextStream& stream, const QnScheduleTask& data)
{
    QString recTypeStr;
    if (data.getRecordingType() == QnScheduleTask::RecordingType_Run)
        recTypeStr = "Always";
    if (data.getRecordingType() == QnScheduleTask::RecordingType_MotionOnly)
        recTypeStr = "Motion";
    if (data.getRecordingType() == QnScheduleTask::RecordingType_Never)
        recTypeStr = "Never";

    QString qualityStr;
    switch (data.getStreamQuality())
    {
    case QnQualityLowest:
        qualityStr = "lowest";
        break;
    case QnQualityLow:
        qualityStr = "low";
        break;
    case QnQualityNormal:
        qualityStr = "normal";
        break;
    case QnQualityHigh:
        qualityStr = "high";
        break;
    case QnQualityHighest:
        qualityStr = "highest";
        break;
    case QnQualityPreSeted:
        qualityStr = "presetted";
        break;
    }

    stream << "type=" << recTypeStr << " fps=" << data.getFps() << " quality=" << qualityStr;
    if (data.getRecordingType() == QnScheduleTask::RecordingType_MotionOnly)
        stream << " pre-motion=" << data.getBeforeThreshold() << "post-motion=" << data.getAfterThreshold();
    return stream;
}



typedef QList<QnScheduleTask> QnScheduleTaskList;

Q_DECLARE_TYPEINFO(QnScheduleTask, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnScheduleTask)
Q_DECLARE_METATYPE(QnScheduleTaskList)

#endif // _vms_schedule_task_h
