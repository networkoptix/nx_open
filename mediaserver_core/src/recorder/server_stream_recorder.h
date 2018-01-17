#pragma once

#include <QtCore/QElapsedTimer>

#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>

#include <recording/stream_recorder.h>
#include <core/misc/schedule_task.h>
#include <recorder/device_file_catalog.h>
#include <recording/time_period.h>
#include <motion/motion_estimation.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <recorder/dualstreaming_helper.h>

#include <nx/vms/event/event_fwd.h>

class QnServerStreamRecorder: public QnStreamRecorder
{
    Q_OBJECT
public:
    QnServerStreamRecorder(
        const QnResourcePtr                 &dev,
        QnServer::ChunksCatalog             catalog,
        QnAbstractMediaStreamDataProvider*  mediaProvider
    );

    ~QnServerStreamRecorder();

    void updateCamera(const QnSecurityCamResourcePtr& cameraRes);
    QnScheduleTask currentScheduleTask() const;
    void updateScheduleInfo(qint64 timeMs);

    void setDualStreamingHelper(const QnDualStreamingHelperPtr& helper);
    QnDualStreamingHelperPtr getDualStreamingHelper() const;

    /*
    * Ignore current schedule task param and start (or restart) recording with specified params.
    * Both primary and secondary streams are recorded.
    * Panic mode recording has higher priority (fps may be increased if panic mode activated)
    */
    void startForcedRecording(Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDurationSec);

    /*
    * Switch from forced recording mode to normal recording mode specified by schedule task
    * Panic mode recording has higher priority
    */
    void stopForcedRecording();

    int getFRAfterThreshold() const;
    bool needConfigureProvider() const;

signals:
    void fpsChanged(QnServerStreamRecorder* recorder, float value);
    void motionDetected(QnResourcePtr resource, bool value, qint64 time, QnConstAbstractDataPacketPtr motion);

    void storageFailure(QnResourcePtr mServerRes, qint64 timestamp, nx::vms::event::EventReason reasonCode, QnResourcePtr storageRes);
protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    virtual bool needSaveData(const QnConstAbstractMediaDataPtr& media) override;
    virtual void beforeProcessData(const QnConstAbstractMediaDataPtr& media);
    virtual bool saveMotion(const QnConstMetaDataV1Ptr& motion) override;

    virtual void fileStarted(
        qint64 startTimeMs,
        int timeZone,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider,
        bool sideRecorder = false) override;

    virtual void fileFinished(
        qint64 durationMs,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider,
        qint64 fileSize,
        qint64 startTimeMs = AV_NOPTS_VALUE) override;

    virtual void getStoragesAndFileNames(QnAbstractMediaStreamDataProvider* provider) override;
    virtual bool canAcceptData() const;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    virtual void setLastMediaTime(qint64 lastMediaTime);

    virtual void beforeRun() override;
    virtual void endOfRun() override;
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md) override;
    virtual void writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex) override;
private:
    void updateRecordingType(const QnScheduleTask& scheduleTask);
    void updateStreamParams();
    bool isMotionRec(Qn::RecordingType recType) const;
    void updateMotionStateInternal(bool value, qint64 timestamp, const QnConstMetaDataV1Ptr& metaData);
    void setSpecialRecordingMode(QnScheduleTask& task);
    int getFpsForValue(int fps);
    void writeRecentlyMotion(qint64 writeAfterTime);
    void keepRecentlyMotion(const QnConstAbstractMediaDataPtr& md);
    bool isPanicMode() const;
    bool isRedundantSyncOn() const;
    bool pauseRebuildIfHighData(QnMutexLockerBase* locker);
    bool resumeRebuildIfLowData(QnMutexLockerBase* locker);
    void resumeRebuild(QnMutexLockerBase* locker);
    void fileCreated(uintptr_t filePtr) const;
    int getBufferSize() const;
    virtual void initIoContext(
        const QnStorageResourcePtr& storage,
        const QString& url,
        AVIOContext** context) override;

    void updateRebuildState();
    void pauseRebuildIfHighDataNoLock();
    void resumeRebuildIfLowDataNoLock();
    bool cleanupQueueIfOverflow();
    void addQueueSizeUnsafe(qint64 value);
    virtual void updateContainerMetadata(QnAviArchiveMetadata* metadata) const override;

private slots:
    void at_recordingFinished(const StreamRecorderErrorStruct& status,
        const QString& filename);

    void at_camera_propertyChanged(const QnResourcePtr &, const QString &);
private:
    const qint64 m_maxRecordQueueSizeBytes;
    const int m_maxRecordQueueSizeElements;
    mutable QnMutex m_scheduleMutex;
    QnScheduleTaskList m_schedule;
    QnTimePeriod m_lastSchedulePeriod;
    QnScheduleTask m_currentScheduleTask;
    //qint64 m_skipDataToTime;
    qint64 m_lastMotionTimeUsec;
    //bool m_lastMotionContainData;
    //bool m_needUpdateStreamParams;
    mutable qint64 m_lastWarningTime;
    QnServer::ChunksCatalog m_catalog;
    QnAbstractMediaStreamDataProvider* m_mediaProvider;
    QnDualStreamingHelperPtr m_dualStreamingHelper;
    QnMediaServerResourcePtr m_mediaServer;
    QnScheduleTask m_panicSchedileRecord;   // panic mode. Highest recording priority
    QnScheduleTask m_forcedSchedileRecord;  // special recording mode (recording action). Priority higher than regular schedule
    QElapsedTimer m_forcedSchedileRecordTimer;
    int m_forcedScheduleRecordDurationMs;
    bool m_usedPanicMode;
    bool m_usedSpecialRecordingMode;
    bool m_lastMotionState; // true if motion in progress
    qint64 m_queuedSize;
    static std::atomic<qint64> m_totalQueueSize;
    static std::atomic<int> m_totalRecorders;
    QnMutex m_queueSizeMutex;
    qint64 m_lastMediaTime;
    QQueue<QnConstAbstractMediaDataPtr> m_recentlyMotion;
    bool m_diskErrorWarned;
    std::atomic<bool> m_rebuildBlocked;
    bool m_usePrimaryRecorder;
    bool m_useSecondaryRecorder;
};
