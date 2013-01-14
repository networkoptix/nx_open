#ifndef __SERVER_STREAM_RECORDER_H__
#define __SERVER_STREAM_RECORDER_H__

#include "recording/stream_recorder.h"
#include "core/misc/schedule_task.h"
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

    /*
    * Ignore current schedule task param and start (or restart) recording with specified params. 
    * Both primary and secondary streams are recorded.
    * Panic mode recording has higher priority (fps may be increased if panic mode activated)
    */
    void startForcedRecording(QnStreamQuality quality = QnQualityHighest, int fps = 10, int maxDuration = 0);

    /*
    * Switch from forced recording mode to normal recording mode specified by schedule task
    * Panic mode recording has higher priority
    */
    void stopForcedRecording();
signals:
    void fpsChanged(QnServerStreamRecorder* recorder, float value);
    void motionDetected(QnResourcePtr resource, bool value, qint64 time, QnMetaDataV1Ptr motion);

    void storageFailure(QnResourcePtr resource, qint64 timestamp, const QString& errMessage);
protected:
    virtual bool processData(QnAbstractDataPacketPtr data);

    virtual bool needSaveData(QnAbstractMediaDataPtr media);
    void beforeProcessData(QnAbstractMediaDataPtr media);
    virtual bool saveMotion(QnMetaDataV1Ptr motion) override;

    virtual void fileStarted(qint64 startTimeMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider* provider) override;
    virtual void fileFinished(qint64 durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider, qint64 fileSize) override;
    virtual QString fillFileName(QnAbstractMediaStreamDataProvider* provider) override;
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data) override;

    virtual void endOfRun() override;
private:
    void updateRecordingType(const QnScheduleTask& scheduleTask);
    void updateStreamParams();
    bool isMotionRec(Qn::RecordingType recType) const;
    void updateMotionStateInternal(bool value, qint64 timestamp, QnMetaDataV1Ptr metaData);
    void setSpecialRecordingMode(QnScheduleTask& task, int fps);
private slots:
    void at_recordingFailed(QString msg);
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
    QnMediaServerResourcePtr m_mediaServer;
    QnScheduleTask m_panicSchedileRecord;   // panic mode. Highest recording priority
    QnScheduleTask m_forcedSchedileRecord;  // special recording mode (recording action). Priority higher than regular schedule
    bool m_usedPanicMode;
    bool m_usedSpecialRecordingMode;
    int m_forcedRecordFps;  // fps for special recording mode
    bool m_lastMotionState; // true if motion in progress
    qint64 m_queuedSize;
    QMutex m_queueSizeMutex;
};

#endif // __SERVER_STREAM_RECORDER_H__
