#include "server_stream_recorder.h"

#include "motion/motion_helper.h"
#include "storage_manager.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "utils/common/synctime.h"
#include "utils/math/math.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include <business/business_event_connector.h>
#include <business/events/reasoned_business_event.h>
#include "plugins/storage/file_storage/file_storage_resource.h"
#include "core/datapacket/media_data_packet.h"
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include "utils/common/util.h" /* For MAX_FRAME_DURATION, MIN_FRAME_DURATION. */

#include <boost/algorithm/cxx11/any_of.hpp>


static const int MOTION_PREBUFFER_SIZE = 8;

QnServerStreamRecorder::QnServerStreamRecorder(const QnResourcePtr &dev, QnServer::ChunksCatalog catalog, QnAbstractMediaStreamDataProvider* mediaProvider)
:
    QnStreamRecorder(dev),
    m_maxRecordQueueSizeBytes( MSSettings::roSettings()->value( nx_ms_conf::MAX_RECORD_QUEUE_SIZE_BYTES, nx_ms_conf::DEFAULT_MAX_RECORD_QUEUE_SIZE_BYTES ).toULongLong() ),
    m_maxRecordQueueSizeElements( MSSettings::roSettings()->value( nx_ms_conf::MAX_RECORD_QUEUE_SIZE_ELEMENTS, nx_ms_conf::DEFAULT_MAX_RECORD_QUEUE_SIZE_ELEMENTS ).toULongLong() ),
    m_scheduleMutex(QMutex::Recursive),
    m_catalog(catalog),
    m_mediaProvider(mediaProvider),
    m_dualStreamingHelper(0),
    m_usedPanicMode(false),
    m_usedSpecialRecordingMode(false),
    m_lastMotionState(false),
    m_queuedSize(0),
    m_lastMediaTime(AV_NOPTS_VALUE),
    m_diskErrorWarned(false),
    m_rebuildBlocked(false)
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
    scheduleData.m_recordType = Qn::RT_Always;
    scheduleData.m_streamQuality = Qn::QualityHighest;
    scheduleData.m_fps = getFpsForValue(0);

    m_panicSchedileRecord.setData(scheduleData);

    connect(this, &QnStreamRecorder::recordingFinished, this, &QnServerStreamRecorder::at_recordingFinished);

    connect(this, SIGNAL(motionDetected(QnResourcePtr, bool, qint64, QnConstAbstractDataPacketPtr)), qnBusinessRuleConnector, SLOT(at_motionDetected(const QnResourcePtr&, bool, qint64, QnConstAbstractDataPacketPtr)));
    connect(this, SIGNAL(storageFailure(QnResourcePtr, qint64, QnBusiness::EventReason, QnResourcePtr)), qnBusinessRuleConnector, SLOT(at_storageFailure(const QnResourcePtr&, qint64, QnBusiness::EventReason, const QnResourcePtr&)));
    connect(dev.data(), SIGNAL(propertyChanged(const QnResourcePtr &, const QString &)),          this, SLOT(at_camera_propertyChanged(const QnResourcePtr &, const QString &)));
    at_camera_propertyChanged(m_device, QString());
}

QnServerStreamRecorder::~QnServerStreamRecorder()
{
    stop();
}


void QnServerStreamRecorder::at_camera_propertyChanged(const QnResourcePtr &, const QString & key)
{
    const QnPhysicalCameraResource* camera = dynamic_cast<QnPhysicalCameraResource*>(m_device.data());
    m_usePrimaryRecorder = (camera->getProperty(QnMediaResource::dontRecordPrimaryStreamKey()).toInt() == 0);
    m_useSecondaryRecorder = (camera->getProperty(QnMediaResource::dontRecordSecondaryStreamKey()).toInt() == 0);
    
    if (key == QnMediaResource::motionStreamKey()) {
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
        if (liveProvider)
            liveProvider->updateSoftwareMotionStreamNum();
    }
}

void QnServerStreamRecorder::at_recordingFinished(int status, const QString &filename) {
    Q_UNUSED(filename)
    if (status == QnStreamRecorder::NoError)
        return;

    Q_ASSERT(m_mediaServer);
    if (m_mediaServer) {
        if (!m_diskErrorWarned)
            emit storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), QnBusiness::StorageIoErrorReason ,m_storage);
        m_diskErrorWarned = true;
    }
}

bool QnServerStreamRecorder::canAcceptData() const
{
    return true;
    /*
    if (!isRunning())
        return true;

    //bool rez = QnStreamRecorder::canAcceptData();
    bool rez = m_queuedSize <= m_maxRecordQueueSizeBytes && m_dataQueue.size() < m_maxRecordQueueSizeElements;
    

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

void QnServerStreamRecorder::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    if (!isRunning()) 
        return;

    const bool halfQueueReached = m_queuedSize >= m_maxRecordQueueSizeBytes/2 || (size_t)m_dataQueue.size() >= m_maxRecordQueueSizeElements/2;
    if (!m_rebuildBlocked && halfQueueReached)
    {
        m_rebuildBlocked = true;
        DeviceFileCatalog::rebuildPause(this);
    }
    else if (m_rebuildBlocked && !halfQueueReached)
    {
        m_rebuildBlocked = false;
        DeviceFileCatalog::rebuildResume(this);
    }

    bool rez = m_queuedSize <= m_maxRecordQueueSizeBytes && (size_t)m_dataQueue.size() < m_maxRecordQueueSizeElements;
    if (!rez) {
        emit storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), QnBusiness::StorageTooSlowReason, m_storage);

        qWarning() << "HDD/SSD is slowing down recording for camera " << m_device->getUniqueId() << ". "<<m_dataQueue.size()<<" frames have been dropped!";
        markNeedKeyData();
        m_dataQueue.clear();
        QMutexLocker lock(&m_queueSizeMutex);
        m_queuedSize = 0;
        return;
    }

    
    const QnAbstractMediaData* media = dynamic_cast<const QnAbstractMediaData*>(nonConstData.data());
    if (media) {
        QMutexLocker lock(&m_queueSizeMutex);
        m_queuedSize += media->dataSize();
    }
    QnStreamRecorder::putData(nonConstData);
}

bool QnServerStreamRecorder::saveMotion(const QnConstMetaDataV1Ptr& motion)
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
    const QnPhysicalCameraResource* camera = dynamic_cast<const QnPhysicalCameraResource*>(m_device.data());
    if (m_mediaProvider)
    {
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
        if (m_catalog == QnServer::HiQualityCatalog) {
            if (m_currentScheduleTask.getRecordingType() != Qn::RT_Never) {
                liveProvider->setFps(m_currentScheduleTask.getFps());
                liveProvider->setQuality(m_currentScheduleTask.getStreamQuality());
            }
            else {
                Q_ASSERT(camera);
                liveProvider->setFps(camera->getMaxFps()-5);
                liveProvider->setQuality(Qn::QualityHighest);
            }
            liveProvider->setSecondaryQuality(camera->isCameraControlDisabled() ? Qn::SSQualityNotDefined : camera->secondaryStreamQuality());
        }
        liveProvider->setCameraControlDisabled(camera->isCameraControlDisabled());
    }
}

bool QnServerStreamRecorder::isMotionRec(Qn::RecordingType recType) const
{
    const QnSecurityCamResource* camera = static_cast<const QnPhysicalCameraResource*>(m_device.data());
    return recType == Qn::RT_MotionOnly || 
           (m_catalog == QnServer::HiQualityCatalog && recType == Qn::RT_MotionAndLowQuality && camera->hasDualStreaming2());
}

void QnServerStreamRecorder::beforeProcessData(const QnConstAbstractMediaDataPtr& media)
{
    m_lastMediaTime = media->timestamp;

    Q_ASSERT_X(m_dualStreamingHelper, Q_FUNC_INFO, "Dual streaming helper must be defined!");
    QnConstMetaDataV1Ptr metaData = qSharedPointerDynamicCast<const QnMetaDataV1>(media);
    if (metaData) {
        m_dualStreamingHelper->onMotion(metaData.data());
        qint64 motionTime = m_dualStreamingHelper->getLastMotionTime();
        if (!metaData->isEmpty()) {
            updateMotionStateInternal(true, metaData->timestamp, metaData);
            m_lastMotionTimeUsec = motionTime;
        }
        return;
    }

    const QnScheduleTask task = currentScheduleTask();
    bool isRecording = task.getRecordingType() != Qn::RT_Never && qnStorageMan->isWritableStoragesAvailable();
    if (!m_device->hasFlags(Qn::foreigner)) {
        if (isRecording) {
            if(m_device->getStatus() == Qn::Online)
                m_device->setStatus(Qn::Recording);
        }
        else {
            if(m_device->getStatus() == Qn::Recording)
                m_device->setStatus(Qn::Online);
        }
    }

    if (!isMotionRec(task.getRecordingType()))
        return;

    qint64 motionTime = m_dualStreamingHelper->getLastMotionTime();
    if (motionTime == (qint64)AV_NOPTS_VALUE)
    {
        setPrebufferingUsec(task.getBeforeThreshold()*1000000ll); // no more motion, set prebuffer again
    }
    else
    {
        m_lastMotionTimeUsec = motionTime;
        setPrebufferingUsec(0); // motion in progress, flush prebuffer
    }
}

void QnServerStreamRecorder::updateMotionStateInternal(bool value, qint64 timestamp, const QnConstMetaDataV1Ptr& metaData)
{
    if (m_lastMotionState == value && !value)
        return;
    m_lastMotionState = value;
    emit motionDetected(getResource(), m_lastMotionState, timestamp, metaData);
}

bool QnServerStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& media)
{
    qint64 afterThreshold = 5 * 1000000ll;
    Qn::RecordingType recType = m_currentScheduleTask.getRecordingType();
    if (recType == Qn::RT_MotionOnly || recType == Qn::RT_MotionAndLowQuality)
        afterThreshold = m_currentScheduleTask.getAfterThreshold()*1000000ll;
    bool isMotionContinue = m_lastMotionTimeUsec != (qint64)AV_NOPTS_VALUE && media->timestamp < m_lastMotionTimeUsec + afterThreshold;
    if (!isMotionContinue)
    {
        if (m_endDateTime == (qint64)AV_NOPTS_VALUE || media->timestamp - m_endDateTime < MAX_FRAME_DURATION*1000)
            updateMotionStateInternal(false, media->timestamp, QnMetaDataV1Ptr());
        else
            updateMotionStateInternal(false, m_endDateTime + MIN_FRAME_DURATION, QnMetaDataV1Ptr());
    }
    QnScheduleTask task = currentScheduleTask();

    const QnSecurityCamResource* camera = static_cast<const QnSecurityCamResource*>(m_device.data());
    const QnMetaDataV1* metaData = dynamic_cast<const QnMetaDataV1*>(media.data());

    if (m_catalog == QnServer::LowQualityCatalog && !metaData && !m_useSecondaryRecorder)
    {
        close();
        return false;
    }
    
    if (m_catalog == QnServer::HiQualityCatalog && !metaData && !m_usePrimaryRecorder)
    {
        close();
        return false;
    }

    if (metaData && !m_useSecondaryRecorder && !m_usePrimaryRecorder) {
        keepRecentlyMotion(media);
        return false;
    }

    if (task.getRecordingType() == Qn::RT_Always)
        return true;
    else if (task.getRecordingType() == Qn::RT_MotionAndLowQuality && (m_catalog == QnServer::LowQualityCatalog || !camera->hasDualStreaming2()))
        return true;
    else if (task.getRecordingType() == Qn::RT_Never)
    {
        close();
        if (media->dataType == QnAbstractMediaData::META_V1)
            keepRecentlyMotion(media);
        return false;
    }
    
    if (metaData)
        return true;

    // write motion only
    // if prebuffering mode and all buffer is full - drop data

    //qDebug() << "needSaveData=" << rez << "df=" << (media->timestamp - (m_lastMotionTimeUsec + task.getAfterThreshold()*1000000ll))/1000000.0;
    if (!isMotionContinue && m_endDateTime != (qint64)AV_NOPTS_VALUE)
    {
        if (media->timestamp - m_endDateTime < MAX_FRAME_DURATION*1000)
            m_endDateTime = media->timestamp;
        else
            m_endDateTime += MIN_FRAME_DURATION;
        close();
    }
    return isMotionContinue;
}

int QnServerStreamRecorder::getFpsForValue(int fps)
{
    const QnPhysicalCameraResource* camera = dynamic_cast<const QnPhysicalCameraResource*>(m_device.data());
    if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing)
    {
        if (m_catalog == QnServer::HiQualityCatalog)
            return fps ? qMin(fps, camera->getMaxFps()-2) : camera->getMaxFps()-2;
        else
            return fps ? qMax(2, qMin(camera->desiredSecondStreamFps(), camera->getMaxFps()-fps)) : 2;
    }
    else
    {
        if (m_catalog == QnServer::HiQualityCatalog)
            return fps ? fps : camera->getMaxFps();
        else
            return camera->desiredSecondStreamFps();
    }
}

void QnServerStreamRecorder::startForcedRecording(Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDuration)
{
    Q_UNUSED(beforeThreshold)

    QnScheduleTask::Data scheduleData;
    scheduleData.m_startTime = 0;
    scheduleData.m_endTime = 24*3600*7;
    scheduleData.m_beforeThreshold = 0; // beforeThreshold not used now
    scheduleData.m_afterThreshold = afterThreshold;
    scheduleData.m_fps = getFpsForValue(fps);
    if (maxDuration) {
        QDateTime dt = qnSyncTime->currentDateTime();
        int currentWeekSeconds = (dt.date().dayOfWeek()-1)*3600*24 + dt.time().hour()*3600 + dt.time().minute()*60 +  dt.time().second();
        scheduleData.m_endTime = currentWeekSeconds + maxDuration;
    }
    scheduleData.m_recordType = Qn::RT_Always;
    scheduleData.m_streamQuality = quality;
    
    m_forcedSchedileRecord.setData(scheduleData);

    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());
}

void QnServerStreamRecorder::stopForcedRecording()
{
    m_forcedSchedileRecord.setEndTime(0);
    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());
}

void QnServerStreamRecorder::updateRecordingType(const QnScheduleTask& scheduleTask)
{
    /*
    QString msg;
    QTextStream str(&msg);
    str << "Update recording params for camera " << m_device->getUniqueId() << "  " << scheduleTask;
    str.flush();
    NX_LOG(msg, cl_logINFO);
    */

    if (!isMotionRec(scheduleTask.getRecordingType()))
    {
        // switch from motion to non-motion recording
        const QnResourcePtr& res = getResource();
        const QnSecurityCamResource* camRes = dynamic_cast<const QnSecurityCamResource*>(res.data());
        bool isNoRec = scheduleTask.getRecordingType() == Qn::RT_Never;
        bool usedInRecordAction = camRes && camRes->isRecordingEventAttached();
        int prebuffer = usedInRecordAction && isNoRec ? 1 : 0; // prebuffer 1 usec if camera used in recording action (for keeping last GOP)
        setPrebufferingUsec(prebuffer);
    }
    else if (getPrebufferingUsec() != 0 || !isMotionRec(m_currentScheduleTask.getRecordingType())) {
        // do not change prebuffer if previous recording is motion and motion in progress
        setPrebufferingUsec(scheduleTask.getBeforeThreshold()*1000000ll);
    }
    m_currentScheduleTask = scheduleTask;
}

void QnServerStreamRecorder::setSpecialRecordingMode(QnScheduleTask& task)
{
    // zero fps value means 'autodetect as maximum possible fps'

    // If stream already recording, do not change params in panic mode because if ServerPush provider has some large reopening time
    //CLServerPushStreamReader* sPushProvider = dynamic_cast<CLServerPushStreamReader*> (m_mediaProvider);
    bool doNotChangeParams = false; //sPushProvider && sPushProvider->isStreamOpened() && m_currentScheduleTask->getFps() >= m_panicSchedileRecord.getFps()*0.75;
    updateRecordingType(task);
    if (!doNotChangeParams)
        updateStreamParams();
    m_lastSchedulePeriod.clear();
}

bool QnServerStreamRecorder::isPanicMode() const
{
    return boost::algorithm::any_of(qnResPool->getAllServers(), [](const QnMediaServerResourcePtr& server) {
        return server->getPanicMode() != Qn::PM_None && server->getStatus() == Qn::Online;
    });
}

void QnServerStreamRecorder::updateScheduleInfo(qint64 timeMs)
{
    QMutexLocker lock(&m_scheduleMutex);

    if (isPanicMode())
    {
        if (!m_usedPanicMode)
        {
            setSpecialRecordingMode(m_panicSchedileRecord);
            m_usedPanicMode = true;
        }
        return;
    }
    else if (!m_forcedSchedileRecord.isEmpty())
    {
        if (!m_usedSpecialRecordingMode)
        {
            setSpecialRecordingMode(m_forcedSchedileRecord);
            m_usedSpecialRecordingMode = true;
        }
        return;
    }

    m_usedSpecialRecordingMode = m_usedPanicMode = false;
    QnScheduleTask noRecordTask(QnUuid(), 1, 0, 0, Qn::RT_Never, 0, 0);

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
                updateRecordingType(noRecordTask);
            }
            static const qint64 SCHEDULE_AGGREGATION = 1000*60*15;
            m_lastSchedulePeriod = QnTimePeriod(qFloor(timeMs, SCHEDULE_AGGREGATION)-MAX_FRAME_DURATION, SCHEDULE_AGGREGATION+MAX_FRAME_DURATION); // check period each 15 min
        }
    }
    else {
        updateRecordingType(noRecordTask);
    }
}

bool QnServerStreamRecorder::processData(const QnAbstractDataPacketPtr& data)
{
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true; // skip data

    {
        QMutexLocker lock(&m_queueSizeMutex);
        m_queuedSize -= media->dataSize();
    }

    // for empty schedule we record all time
    beforeProcessData(media);
    bool rez = QnStreamRecorder::processData(data);
    return rez;
}

void QnServerStreamRecorder::updateCamera(const QnSecurityCamResourcePtr& cameraRes)
{
    QMutexLocker lock(&m_scheduleMutex);
    m_schedule = cameraRes->getScheduleTasks();
    Q_ASSERT_X(m_dualStreamingHelper, Q_FUNC_INFO, "DialStreaming helper must be defined!");
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
        return qnStorageMan->getFileName(m_startDateTime/1000, m_currentTimeZone, netResource, DeviceFileCatalog::prefixByCatalog(m_catalog), m_storage);
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
    updateMotionStateInternal(false, m_lastMediaTime, QnMetaDataV1Ptr());

    QnStreamRecorder::endOfRun();
    if(m_device->getStatus() == Qn::Recording)
        m_device->setStatus(Qn::Online);

    if (m_rebuildBlocked) {
        m_rebuildBlocked = false;
        DeviceFileCatalog::rebuildResume(this);
    }

    QMutexLocker lock(&m_queueSizeMutex);
    m_dataQueue.clear();
    m_queuedSize = 0;
}

void QnServerStreamRecorder::setDualStreamingHelper(const QnDualStreamingHelperPtr& helper)
{
    m_dualStreamingHelper = helper;
}

QnDualStreamingHelperPtr QnServerStreamRecorder::getDualStreamingHelper() const
{
    return m_dualStreamingHelper;
}

int QnServerStreamRecorder::getFRAfterThreshold() const
{
    return m_forcedSchedileRecord.getAfterThreshold();
}

void QnServerStreamRecorder::writeRecentlyMotion(qint64 writeAfterTime)
{
    writeAfterTime -= MOTION_AGGREGATION_PERIOD;
    for (int i = 0; i < m_recentlyMotion.size(); ++i)
    {
        if (m_recentlyMotion[i]->timestamp > writeAfterTime)
            QnStreamRecorder::saveData(m_recentlyMotion[i]);
    }
    m_recentlyMotion.clear();
}

void QnServerStreamRecorder::keepRecentlyMotion(const QnConstAbstractMediaDataPtr& md)
{
    if (m_recentlyMotion.size() == MOTION_PREBUFFER_SIZE)
        m_recentlyMotion.dequeue();
    m_recentlyMotion.enqueue(md);
}

bool QnServerStreamRecorder::saveData(const QnConstAbstractMediaDataPtr& md)
{
    writeRecentlyMotion(md->timestamp);
    return QnStreamRecorder::saveData(md);
}

void QnServerStreamRecorder::writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex)
{
    QnStreamRecorder::writeData(md, streamIndex);
    m_diskErrorWarned = false;
}

bool QnServerStreamRecorder::needConfigureProvider() const
{
    const QnPhysicalCameraResource* camera = dynamic_cast<QnPhysicalCameraResource*>(m_device.data());
    return !camera->isScheduleDisabled();
}
