#ifndef __SERVER_STREAM_RECORDER_H__
#define __SERVER_STREAM_RECORDER_H__

#include "recording/stream_recorder.h"
#include "core/misc/scheduleTask.h"
#include "recorder/device_file_catalog.h"
#include "recording/time_period.h"

class QnServerStreamRecorder: public QnStreamRecorder
{
public:
    QnServerStreamRecorder(QnResourcePtr dev);

    void updateSchedule(const QnScheduleTaskList& schedule);
protected:
    virtual bool processData(QnAbstractDataPacketPtr data);

    virtual bool needSaveData(QnAbstractMediaDataPtr media);
    void beforeProcessData(QnAbstractMediaDataPtr media);
    bool saveMotion(QnAbstractMediaDataPtr media);

    virtual void fileStarted(qint64 startTimeMs, const QString& fileName);
    virtual void fileFinished(qint64 durationMs, const QString& fileName);
    virtual QString fillFileName();
private:
    void updateRecordingType(const QnScheduleTask& scheduleTask);
private:
    QMutex m_scheduleMutex;
    QnScheduleTaskList m_schedule;
    QnTimePeriod m_lastSchedulePeriod;
    QnScheduleTask m_currentScheduleTask;
    //qint64 m_skipDataToTime;
    qint64 m_lastMotionTimeUsec;
    bool m_lastMotionContainData;
    bool m_needUpdateStreamParams;
};

#endif // __SERVER_STREAM_RECORDER_H__
