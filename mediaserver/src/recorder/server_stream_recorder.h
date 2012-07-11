#ifndef __SERVER_STREAM_RECORDER_H__
#define __SERVER_STREAM_RECORDER_H__

#include "recording/stream_recorder.h"
#include "core/misc/scheduleTask.h"
#include "recorder/device_file_catalog.h"
#include "recording/time_period.h"
#include "motion/motion_estimation.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "dualstreaming_helper.h"

class QnServerStreamRecorder: public QnStreamRecorder
{
    Q_OBJECT
public:
    QnServerStreamRecorder(QnResourcePtr dev, QnResource::ConnectionRole role, QnAbstractMediaStreamDataProvider* mediaProvider);
    ~QnServerStreamRecorder();

    void updateCamera(QnSecurityCamResourcePtr cameraRes);
    QnScheduleTask currentScheduleTask() const;
    void updateScheduleInfo(qint64 timeMs);

    void setDualStreamingHelper(QnDualStreamingHelperPtr helper);
signals:
    void fpsChanged(QnServerStreamRecorder* recorder, float value);
protected:
    virtual bool processData(QnAbstractDataPacketPtr data);

    virtual bool needSaveData(QnAbstractMediaDataPtr media);
    void beforeProcessData(QnAbstractMediaDataPtr media);
    bool saveMotion(QnAbstractMediaDataPtr media);

    virtual void fileStarted(qint64 startTimeMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider) override;
    virtual void fileFinished(qint64 durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider, qint64 fileSize) override;
    virtual QString fillFileName(QnAbstractMediaStreamDataProvider* provider) override;
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data) override;

    virtual void endOfRun() override;
private:
    void updateRecordingType(const QnScheduleTask& scheduleTask);
    void updateStreamParams();
    bool isMotionRec(QnScheduleTask::RecordingType recType) const;
private:
    mutable QMutex m_scheduleMutex;
    QnScheduleTaskList m_schedule;
    QnTimePeriod m_lastSchedulePeriod;
    QnScheduleTask m_currentScheduleTask;
    //qint64 m_skipDataToTime;
    qint64 m_lastMotionTimeUsec;
    //bool m_lastMotionContainData;
    //bool m_needUpdateStreamParams;
    mutable qint64 m_lastWarningTime;
    QnResource::ConnectionRole m_role;
    QnAbstractMediaStreamDataProvider* m_mediaProvider;
    QnDualStreamingHelperPtr m_dualStreamingHelper;
};

#endif // __SERVER_STREAM_RECORDER_H__
