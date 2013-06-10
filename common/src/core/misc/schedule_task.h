#ifndef _vms_schedule_task_h
#define _vms_schedule_task_h

#include <QSharedPointer>
#include <QList>
#include <QTextStream>

#include "utils/common/id.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "schedule_recording_type.h"

class QnScheduleTask
{
public:
    struct Data
    {
        Data(int dayOfWeek = 1, int startTime = 0, int endTime = 0, Qn::RecordingType recordType = Qn::RecordingType_Never,
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

        /** Day of the week, integer in range [1..7]. */
        int m_dayOfWeek;

        /** Start time offset, in seconds. */
        int m_startTime;

        /** End time offset, in seconds. */
        int m_endTime;

        Qn::RecordingType m_recordType;

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
                   Qn::RecordingType recordType =  Qn::RecordingType_Never, int beforeThreshold = 0, int afterThreshold = 0,
                   QnStreamQuality streamQuality = QnQualityHighest, int fps = 10, bool doRecordAudio = false)
        : m_id(id), m_resourceId(resourceId),
          m_data(dayOfWeek, startTime, endTime, recordType, beforeThreshold, afterThreshold, streamQuality, fps, doRecordAudio)
    {}

    bool isEmpty() const {return m_data.m_startTime == 0 && m_data.m_endTime == 0; }

    const Data& getData() const { return m_data; }
    void setData(const Data& data) { m_data = data; }

    QnId getId() const { return m_id; }
    QnId getResourceId() const { return m_resourceId; }
    int getDayOfWeek() const { return m_data.m_dayOfWeek; }
    int getStartTime() const { return m_data.m_startTime; }
    int getHour() const;
    int getMinute() const;
    int getSecond() const;
    int getEndTime() const { return m_data.m_endTime; }
    void setEndTime(int value) { m_data.m_endTime = value; }

    Qn::RecordingType getRecordingType() const { return m_data.m_recordType; }
    int getBeforeThreshold() const { return m_data.m_beforeThreshold; }
    void setBeforeThreshold(int value)  { m_data.m_beforeThreshold = value; }
    int getAfterThreshold() const { return m_data.m_afterThreshold; }
    void setAfterThreshold(int value) { m_data.m_afterThreshold= value; }
    QnStreamQuality getStreamQuality() const { return m_data.m_streamQuality; }
    void setStreamQuality(QnStreamQuality value) { m_data.m_streamQuality = value; }
    
    int getFps() const { return m_data.m_fps; }
    void setFps(int value) { m_data.m_fps = value; }
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

    friend class QnCameraScheduleWidget; // TODO: #vasilenko what the hell?
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
    QString recordingTypeString;
    switch(data.getRecordingType()) {
    case Qn::RecordingType_Run:
        recordingTypeString = QLatin1String("Always");
        break;
    case Qn::RecordingType_MotionOnly:
        recordingTypeString = QLatin1String("Motion");
        break;
    case Qn::RecordingType_Never:
        recordingTypeString = QLatin1String("Never");
        break;
    case Qn::RecordingType_MotionPlusLQ:
        recordingTypeString = QLatin1String("MotionAndLQ");
        break;
    default:
        break;
    }

    QString qualityString;
    switch (data.getStreamQuality()) {
    case QnQualityLowest:
        qualityString = QLatin1String("lowest");
        break;
    case QnQualityLow:
        qualityString = QLatin1String("low");
        break;
    case QnQualityNormal:
        qualityString = QLatin1String("normal");
        break;
    case QnQualityHigh:
        qualityString = QLatin1String("high");
        break;
    case QnQualityHighest:
        qualityString = QLatin1String("highest");
        break;
    case QnQualityPreSet:
        qualityString = QLatin1String("preset");
        break;
    }

    stream << "type=" << recordingTypeString << " fps=" << data.getFps() << " quality=" << qualityString;
    if (data.getRecordingType() == Qn::RecordingType_MotionOnly)
        stream << " pre-motion=" << data.getBeforeThreshold() << "post-motion=" << data.getAfterThreshold();
    return stream;
}


typedef QList<QnScheduleTask> QnScheduleTaskList;

Q_DECLARE_TYPEINFO(QnScheduleTask, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnScheduleTask)
Q_DECLARE_METATYPE(QnScheduleTaskList)

#endif // _vms_schedule_task_h
