#include "server_stream_recorder.h"
#include "motion/motion_helper.h"
#include "storage_manager.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/camera_resource.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include "core/datapacket/media_data_packet.h"

static const int MAX_BUFFERED_SIZE = 1024*1024*20;

QnServerStreamRecorder::QnServerStreamRecorder(QnResourcePtr dev, QnResource::ConnectionRole role, QnAbstractMediaStreamDataProvider* mediaProvider):
    QnStreamRecorder(dev),
    m_role(role),
    m_mediaProvider(mediaProvider),
    m_scheduleMutex(QMutex::Recursive),
    m_dualStreamingHelper(0),
    m_usedPanicMode(false),
    m_queuedSize(0)
{
    //m_skipDataToTime = AV_NOPTS_VALUE;
    m_lastMotionTimeUsec = AV_NOPTS_VALUE;
    //m_needUpdateStreamParams = true;
    m_lastWarningTime = 0;
    m_stopOnWriteError = false;
    m_mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(getResource()->getParentId()));
    
    QnScheduleTask::Data scheduleData;
    scheduleData.m_startTime = 0;
    scheduleData.m_endTime = 24*3600*7;
    scheduleData.m_recordType = QnScheduleTask::RecordingType_Run;
    scheduleData.m_streamQuality = QnQualityHighest;
    m_panicSchedileRecord.setData(scheduleData);
}

QnServerStreamRecorder::~QnServerStreamRecorder()
{
    stop();
}

bool QnServerStreamRecorder::canAcceptData() const
{
    return true;
    /*
    if (!isRunning())
        return true;

    //bool rez = QnStreamRecorder::canAcceptData();
    bool rez = m_queuedSize <= MAX_BUFFERED_SIZE && m_dataQueue.size() < 1000;
    

    if (!rez) {
        qint64 currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        if (currentTime - m_lastWarningTime > 1000)
        {
            qWarning() << "HDD/SSD is slow down recording for camera " << m_device->getUniqueId() << "frame rate decreased!";
            m_lastWarningTime = currentTime;
        }
    }
    return rez;
    */
}

void QnServerStreamRecorder::putData(QnAbstractDataPacketPtr data)
{
    if (!isRunning()) 
        return;

    bool rez = m_queuedSize <= MAX_BUFFERED_SIZE && m_dataQueue.size() < 1000;
    if (!rez) {
        qint64 currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        if (currentTime - m_lastWarningTime > 1000)
        {
            qWarning() << "HDD/SSD is slow down recording for camera " << m_device->getUniqueId() << "frame rate decreased!";
            m_lastWarningTime = currentTime;
        }
        markNeedKeyData();
        return;
    }

    
    QnAbstractMediaDataPtr media = data.dynamicCast<QnAbstractMediaData>();
    if (media) {
        QMutexLocker lock(&m_queueSizeMutex);
        m_queuedSize += media->data.size();
    }
    QnStreamRecorder::putData(data);
}

bool QnServerStreamRecorder::saveMotion(QnMetaDataV1Ptr motion)
{
    if (motion)
        QnMotionHelper::instance()->saveToArchive(motion);
    return true;
}

QnScheduleTask QnServerStreamRecorder::currentScheduleTask() const
{
    QMutexLocker lock(&m_scheduleMutex);
    return m_currentScheduleTask;
}

void QnServerStreamRecorder::updateStreamParams()
{
    if (m_mediaProvider && m_role == QnResource::Role_LiveVideo)
    {
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);

        if (m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_Never) {
            liveProvider->setFps(m_currentScheduleTask.getFps());
            liveProvider->setQuality(m_currentScheduleTask.getStreamQuality());
        }
        else {
            QnPhysicalCameraResourcePtr camera = qSharedPointerDynamicCast<QnPhysicalCameraResource>(m_device);
            Q_ASSERT(camera);
            liveProvider->setFps(camera->getMaxFps()-5);
            liveProvider->setQuality(QnQualityHighest);
        }
    }
}

bool QnServerStreamRecorder::isMotionRec(QnScheduleTask::RecordingType recType) const
{
    return recType == QnScheduleTask::RecordingType_MotionOnly || 
           m_role == QnResource::Role_LiveVideo && recType == QnScheduleTask::RecordingType_MotionPlusLQ;
}

void QnServerStreamRecorder::beforeProcessData(QnAbstractMediaDataPtr media)
{
    Q_ASSERT_X(m_dualStreamingHelper, Q_FUNC_INFO, "Dual streaming helper must be defined!");
    QnMetaDataV1Ptr metaData = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (metaData) {
        m_dualStreamingHelper->onMotion(metaData);
        return;
    }

	const QnScheduleTask task = currentScheduleTask();
    bool isRecording = task.getRecordingType() != QnScheduleTask::RecordingType_Never;
    if (!m_device->isDisabled()) {
        if (isRecording) {
            if(m_device->getStatus() == QnResource::Online)
                m_device->setStatus(QnResource::Recording);
        }
        else {
            if(m_device->getStatus() == QnResource::Recording)
                m_device->setStatus(QnResource::Online);
        }
    }

    if (!isMotionRec(task.getRecordingType()))
        return;

    qint64 motionTime = m_dualStreamingHelper->getLastMotionTime();
    if (motionTime == AV_NOPTS_VALUE) 
    {
        setPrebufferingUsec(task.getBeforeThreshold()*1000000ll); // no more motion, set prebuffer again
    }
    else
    {
        m_lastMotionTimeUsec = motionTime;
        setPrebufferingUsec(0); // motion in progress, flush prebuffer
    }

    /*
    QnMetaDataV1Ptr metaData = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (metaData) {
        
        metaData->removeMotion(m_motionMaskBinData[metaData->channelNumber]);
        bool motionContainData = !metaData->isEmpty();
        if (motionContainData || m_lastMotionContainData) {
            m_lastMotionTimeUsec = metaData->timestamp;
            setPrebufferingUsec(0); // flush prebuffer
        }
        m_lastMotionContainData = motionContainData;
        return;
    }
    if (m_lastMotionTimeUsec != AV_NOPTS_VALUE && media->timestamp >= m_lastMotionTimeUsec + m_currentScheduleTask.getAfterThreshold()*1000000ll)
    {
        // no motion sometime. Go to prebuffering mode
        setPrebufferingUsec(m_currentScheduleTask.getBeforeThreshold()*1000000ll);
    }
    */
}

bool QnServerStreamRecorder::needSaveData(QnAbstractMediaDataPtr media)
{
	QnScheduleTask task = currentScheduleTask();

    if (task.getRecordingType() == QnScheduleTask::RecordingType_Run)
        return true;
    else if (task.getRecordingType() == QnScheduleTask::RecordingType_MotionPlusLQ && m_role == QnResource::Role_SecondaryLiveVideo)
        return true;
    else if (task.getRecordingType() == QnScheduleTask::RecordingType_Never)
    {
        close();
        return false;
    }
    
    QnMetaDataV1Ptr metaData = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (metaData)
        return true;

    // write motion only
    // if prebuffering mode and all buffer is full - drop data

    bool rez = m_lastMotionTimeUsec != AV_NOPTS_VALUE && media->timestamp < m_lastMotionTimeUsec + task.getAfterThreshold()*1000000ll;
    //qDebug() << "needSaveData=" << rez << "df=" << (media->timestamp - (m_lastMotionTimeUsec + task.getAfterThreshold()*1000000ll))/1000000.0;
    if (!rez && m_endDateTime != AV_NOPTS_VALUE) 
    {
        if (media->timestamp - m_endDateTime < MAX_FRAME_DURATION*1000)
            m_endDateTime = media->timestamp;
        else
            m_endDateTime += MIN_FRAME_DURATION;
        close();
    }
    return rez;
}

void QnServerStreamRecorder::updateRecordingType(const QnScheduleTask& scheduleTask)
{
    QString msg;
    QTextStream str(&msg);
    str << "Update recording params for camera " << m_device->getUniqueId() << "  " << scheduleTask;
    str.flush();
    cl_log.log(msg, cl_logINFO);

    if (!isMotionRec(scheduleTask.getRecordingType()))
    {
        // switch from motion to non-motion recording
        flushPrebuffer();
        setPrebufferingUsec(0);
    }
    else if (getPrebufferingUsec() != 0 || !isMotionRec(m_currentScheduleTask.getRecordingType())) {
        // do not change prebuffer if previous recording is motion and motion in progress
        setPrebufferingUsec(scheduleTask.getBeforeThreshold()*1000000ll);
    }
    m_currentScheduleTask = scheduleTask;
}

void QnServerStreamRecorder::updateScheduleInfo(qint64 timeMs)
{
    QMutexLocker lock(&m_scheduleMutex);

    if (m_mediaServer && m_mediaServer->isPanicMode())
    {
        if (!m_usedPanicMode)
        {
            QnPhysicalCameraResourcePtr camera = qSharedPointerDynamicCast<QnPhysicalCameraResource>(m_device);

            if (camera->streamFpsSharingMethod()==shareFps)
            {
                if (m_role == QnResource::Role_LiveVideo)
                    m_panicSchedileRecord.setFps(camera->getMaxFps()-2);
                else
                    m_panicSchedileRecord.setFps(2);
            }
            else
            {
                if (m_role == QnResource::Role_LiveVideo)
                    m_panicSchedileRecord.setFps(camera->getMaxFps());
                else
                    m_panicSchedileRecord.setFps(DESIRED_SECOND_STREAM_FPS);
            }


            // If stream already recording, do not change params in panic mode because if ServerPush provider has some large reopening time
            CLServerPushStreamreader* sPushProvider = dynamic_cast<CLServerPushStreamreader*> (m_mediaProvider);
            bool doNotChangeParams = false; //sPushProvider && sPushProvider->isStreamOpened() && m_currentScheduleTask->getFps() >= m_panicSchedileRecord.getFps()*0.75;
            updateRecordingType(m_panicSchedileRecord);
            if (!doNotChangeParams)
                updateStreamParams();
            m_lastSchedulePeriod.clear();
            m_usedPanicMode = true;
        }
        return;
    }

    m_usedPanicMode = false;

    if (!m_schedule.isEmpty())
    {
        //bool isEmptyPeriod = m_skipDataToTime != AV_NOPTS_VALUE && timeMs < m_skipDataToTime;
        if (!m_lastSchedulePeriod.contains(timeMs))
        {
            // find new schedule
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timeMs);
            int scheduleTimeMs = (dt.date().dayOfWeek()-1)*3600*24 + dt.time().hour()*3600+dt.time().minute()*60+dt.time().second();
            scheduleTimeMs *= 1000;

            QnScheduleTaskList::iterator itr = qUpperBound(m_schedule.begin(), m_schedule.end(), scheduleTimeMs);
            if (itr > m_schedule.begin())
                --itr;

            if (itr->containTimeMs(scheduleTimeMs)) {
                updateRecordingType(*itr);
                updateStreamParams();
            }
            else {
                QnScheduleTask noRecordTask(QnId::generateSpecialId(), m_device->getId(), 1, 0, 0, QnScheduleTask::RecordingType_Never, 0, 0);
                updateRecordingType(noRecordTask);
            }
            static const qint64 SCHEDULE_AGGREGATION = 1000*60*15;
            m_lastSchedulePeriod = QnTimePeriod(qFloor(timeMs, SCHEDULE_AGGREGATION)-MAX_FRAME_DURATION, SCHEDULE_AGGREGATION+MAX_FRAME_DURATION); // check period each 15 min
        }
    }
    else if (m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_Never) 
    {
        updateRecordingType(QnScheduleTask());
    }
}

bool QnServerStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true; // skip data

    {
        QMutexLocker lock(&m_queueSizeMutex);
        m_queuedSize -= media->data.size();
    }

    // for empty schedule we record all time
    beforeProcessData(media);
    return QnStreamRecorder::processData(data);
}

void QnServerStreamRecorder::updateCamera(QnSecurityCamResourcePtr cameraRes)
{
    QMutexLocker lock(&m_scheduleMutex);
    m_schedule = cameraRes->getScheduleTasks();
    Q_ASSERT_X(m_dualStreamingHelper, Q_FUNC_INFO, "DialStreaming helper must be defined!");
    m_dualStreamingHelper->updateCamera(cameraRes);
    m_lastSchedulePeriod.clear();
    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());

    if (m_mediaProvider)
    {   
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
        liveProvider->updateSoftwareMotion();
    }
}

QString QnServerStreamRecorder::fillFileName(QnAbstractMediaStreamDataProvider* provider)
{
    if (m_fixedFileName.isEmpty())
    {
        QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(m_device);
        Q_ASSERT_X(netResource != 0, Q_FUNC_INFO, "Only network resources can be used with storage manager!");
        m_storage = qnStorageMan->getOptimalStorageRoot(provider);
        if (m_storage)
            setTruncateInterval(m_storage->getChunkLen());
        return qnStorageMan->getFileName(m_startDateTime/1000, m_currentTimeZone, netResource, DeviceFileCatalog::prefixForRole(m_role), m_storage);
    }
    else {
        return m_fixedFileName;
    }
}

void QnServerStreamRecorder::fileFinished(qint64 durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider, qint64 fileSize)
{
    if (m_truncateInterval != 0)
        qnStorageMan->fileFinished(durationMs, fileName, provider, fileSize);
};

void QnServerStreamRecorder::fileStarted(qint64 startTimeMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider* provider)
{
    if (m_truncateInterval > 0) {
        qnStorageMan->fileStarted(startTimeMs, timeZone, fileName, provider);
    }
}

void QnServerStreamRecorder::endOfRun()
{
    QnStreamRecorder::endOfRun();
    if(m_device->getStatus() == QnResource::Recording)
        m_device->setStatus(QnResource::Online);

    QMutexLocker lock(&m_queueSizeMutex);
    m_dataQueue.clear();
    m_queuedSize = 0;
}

void QnServerStreamRecorder::setDualStreamingHelper(QnDualStreamingHelperPtr helper)
{
    m_dualStreamingHelper = helper;
}
