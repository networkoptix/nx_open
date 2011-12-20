#ifndef __SERVER_STREAM_RECORDER_H__
#define __SERVER_STREAM_RECORDER_H__

#include "recording/stream_recorder.h"
#include "core/misc/scheduleTask.h"
#include "recording/device_file_catalog.h"

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
};

#endif // __SERVER_STREAM_RECORDER_H__
