#include <limits>
#include <cstdint>
#include "server_stream_recorder.h"

#include <api/global_settings.h>

#include <common/common_module.h>
#include <common/common_globals.h>
#include "motion/motion_helper.h"
#include "storage_manager.h"
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include "providers/live_stream_provider.h"
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include <core/resource/server_backup_schedule.h>

#include "utils/common/synctime.h"
#include "utils/math/math.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "providers/spush_media_stream_provider.h"
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/event/events/reasoned_event.h>
#include "plugins/storage/file_storage/file_storage_resource.h"
#include "nx/streaming/media_data_packet.h"
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include "utils/common/util.h" /* For MAX_FRAME_DURATION_MS, MIN_FRAME_DURATION_USEC. */
#include <recorder/recording_manager.h>
#include <utils/common/buffered_file.h>
#include <utils/media/ffmpeg_helper.h>
#include <media_server/media_server_module.h>
#include <nx/utils/cryptographic_hash.h>
#include "archive_integrity_watcher.h"
#include <nx/vms/server/resource/camera.h>
#include <utils/media/h264_utils.h>
#include <utils/media/nalUnits.h>
#include <utils/media/hevc_common.h>

namespace {

static const int kMotionPrebufferSize = 8;
static const double kHighDataLimit = 0.7;
static const double kLowDataLimit = 0.3;

/*
* Schedule relative start time inside a week at ms.
*/
int taskStartTimeMs(const QnScheduleTask& task)
{
    return 1000 * ((task.dayOfWeek - 1) * 3600 * 24 + task.startTime);
}

bool scheduleTaskContainsWeekTimeMs(const QnScheduleTask& task, int weekTimeMs)
{
    const int startTimeMs = taskStartTimeMs(task);
    const int durationMs = 1000 * (task.endTime - task.startTime);
    return qBetween(startTimeMs, weekTimeMs, startTimeMs + durationMs);
}

} // namespace

std::atomic<qint64> QnServerStreamRecorder::m_totalQueueSize;
std::atomic<int> QnServerStreamRecorder::m_totalRecorders;

QnServerStreamRecorder::QnServerStreamRecorder(
    QnMediaServerModule* serverModule,
    const QnResourcePtr                 &dev,
    QnServer::ChunksCatalog             catalog,
    QnAbstractMediaStreamDataProvider*  mediaProvider)
    :
    QnStreamRecorder(dev),
    m_maxRecordQueueSizeBytes(qnGlobalSettings->maxRecorderQueueSizeBytes()),
    m_maxRecordQueueSizeElements(qnGlobalSettings->maxRecorderQueueSizePackets()),
    m_scheduleMutex(QnMutex::Recursive),
    m_catalog(catalog),
    m_mediaProvider(mediaProvider),
    m_dualStreamingHelper(0),
    m_forcedScheduleRecordDurationMs(0),
    m_usedPanicMode(false),
    m_usedSpecialRecordingMode(false),
    m_lastMotionState(false),
    m_queuedSize(0),
    m_lastMediaTime(AV_NOPTS_VALUE),
    m_diskErrorWarned(false),
    m_rebuildBlocked(false),
    m_canDropPackets(true),
    m_serverModule(serverModule)
{
    //m_skipDataToTime = AV_NOPTS_VALUE;
    m_lastMotionTimeUsec = AV_NOPTS_VALUE;
    //m_needUpdateStreamParams = true;
    m_lastWarningTime = 0;
    m_mediaServer = m_resource->getParentResource().dynamicCast<QnMediaServerResource>();

    m_panicScheduleRecordTask.startTime = 0;
    m_panicScheduleRecordTask.endTime = 24*3600*7;
    m_panicScheduleRecordTask.recordingType = Qn::RecordingType::always;
    m_panicScheduleRecordTask.streamQuality = Qn::StreamQuality::highest;
    m_panicScheduleRecordTask.fps = getFpsForValue(0);

    connect(this, &QnStreamRecorder::recordingFinished, this, &QnServerStreamRecorder::at_recordingFinished);

    connect(this, &QnServerStreamRecorder::motionDetected, m_serverModule->eventConnector(),
        &nx::vms::server::event::EventConnector::at_motionDetected);

    using storageFailureWithResource = void (nx::vms::server::event::EventConnector::*)
        (const QnResourcePtr&, qint64, nx::vms::api::EventReason, const QnResourcePtr&);

    connect(this, &QnServerStreamRecorder::storageFailure, m_serverModule->eventConnector(),
        storageFailureWithResource(&nx::vms::server::event::EventConnector::at_storageFailure));

    connect(dev.data(), &QnResource::propertyChanged, this,
        &QnServerStreamRecorder::at_camera_propertyChanged);

    at_camera_propertyChanged(m_resource, QString());
}

QnServerStreamRecorder::~QnServerStreamRecorder()
{
    m_resource->disconnect(this);
    disconnect();
    stop();
}

void QnServerStreamRecorder::at_camera_propertyChanged(const QnResourcePtr &, const QString & key)
{
    const auto camera = dynamic_cast<nx::vms::server::resource::Camera*>(m_resource.data());
    m_usePrimaryRecorder = (camera->getProperty(QnMediaResource::dontRecordPrimaryStreamKey()).toInt() == 0);
    m_useSecondaryRecorder = (camera->getProperty(QnMediaResource::dontRecordSecondaryStreamKey()).toInt() == 0);

    QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
    if (liveProvider) {
        if (key == QnMediaResource::motionStreamKey())
            liveProvider->updateSoftwareMotionStreamNum();
        else if (key == ResourcePropertyKey::kBitratePerGOP)
            liveProvider->pleaseReopenStream();
    }
}

void QnServerStreamRecorder::at_recordingFinished(const StreamRecorderErrorStruct& status,
    const QString& /*filename*/)
{
    if (status.lastError == StreamRecorderError::noError)
        return;

    NX_ASSERT(m_mediaServer);
    if (m_mediaServer) {
        if (!m_diskErrorWarned)
        {
            if (status.storage)
                emit storageFailure(
                    m_mediaServer,
                    qnSyncTime->currentUSecsSinceEpoch(),
                    nx::vms::api::EventReason::storageIoError,
                    status.storage
                );
            m_diskErrorWarned = true;
        }
    }
}

bool QnServerStreamRecorder::canAcceptData() const
{
    return true;
}

void QnServerStreamRecorder::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    if (!isRunning())
        return;

    if(m_canDropPackets && isQueueFull())
        cleanupQueue();
    updateRebuildState(); //< pause/resume archive rebuild

    const QnAbstractMediaData* media = dynamic_cast<const QnAbstractMediaData*>(nonConstData.get());
    if (media)
    {
        QnMutexLocker lock(&m_queueSizeMutex);
        if (needToStop())
            return;
        addQueueSizeUnsafe(media->dataSize());
        QnStreamRecorder::putData(nonConstData);
    }
}

void QnServerStreamRecorder::setLastMediaTime(qint64 lastMediaTime)
{
    m_lastMediaTime = lastMediaTime;
}

void QnServerStreamRecorder::updateRebuildState()
{
    QnMutexLocker lock( &m_queueSizeMutex );
    pauseRebuildIfHighDataNoLock();
    resumeRebuildIfLowDataNoLock();
}

void QnServerStreamRecorder::pauseRebuildIfHighDataNoLock()
{
    const qint64 totalAllowedBytes = m_maxRecordQueueSizeBytes * m_totalRecorders;
    if (!m_rebuildBlocked && (
        (double) m_totalQueueSize > totalAllowedBytes * kHighDataLimit ||
        (double) m_dataQueue.size() > m_maxRecordQueueSizeElements * kHighDataLimit))
    {
        m_rebuildBlocked = true;
        DeviceFileCatalog::rebuildPause(this);
    }
}

void QnServerStreamRecorder::resumeRebuildIfLowDataNoLock()
{
    const qint64 totalAllowedBytes = m_maxRecordQueueSizeBytes * m_totalRecorders;
    if (m_rebuildBlocked && (
        (double) m_totalQueueSize < totalAllowedBytes * kLowDataLimit &&
        (double) m_dataQueue.size() < m_maxRecordQueueSizeElements * kLowDataLimit))
    {
        m_rebuildBlocked = false;
        DeviceFileCatalog::rebuildResume(this);
    }
}

bool QnServerStreamRecorder::isQueueFull() const
{
    QnMutexLocker lock(&m_queueSizeMutex);

    if (m_dataQueue.size() > m_maxRecordQueueSizeElements)
        return true;

    // check for global overflow bytes between all recorders
    const qint64 totalAllowedBytes = m_maxRecordQueueSizeBytes * m_totalRecorders;
    if (totalAllowedBytes > m_totalQueueSize)
        return false; //< no need to cleanup

    if (m_queuedSize < m_maxRecordQueueSizeBytes)
        return false; //< no need to cleanup

    return true;
}

void QnServerStreamRecorder::setCanDropPackets(bool canDrop)
{
    m_canDropPackets = canDrop;
}

bool QnServerStreamRecorder::canDropPackets() const
{
    return m_canDropPackets;
}

bool QnServerStreamRecorder::cleanupQueue()
{
    {
        QnMutexLocker lock(&m_queueSizeMutex);
        if (!m_recordingContextVector.empty())
        {
            size_t slowestStorageIndex;
            int64_t slowestWriteTime = std::numeric_limits<int64_t>::min();

            for (size_t i = 0; i < m_recordingContextVector.size(); ++i)
            {
                if (m_recordingContextVector[i].totalWriteTimeNs > slowestWriteTime)
                    slowestStorageIndex = i;
            }

            const auto storage = m_recordingContextVector[slowestStorageIndex].storage;
            lock.unlock();
            emit storageFailure(
                m_mediaServer,
                qnSyncTime->currentUSecsSinceEpoch(),
                nx::vms::api::EventReason::storageTooSlow,
                storage);
        }
    }

    qWarning() << "HDD/SSD is slowing down recording for camera " << m_resource->getUniqueId() << ". "<<m_dataQueue.size()<<" frames have been dropped!";
    markNeedKeyData();

    {
        QnMutexLocker lock( &m_queueSizeMutex );
        QnAbstractDataPacketPtr data;
        while (m_dataQueue.pop(data, 0))
        {
            if (auto media = std::dynamic_pointer_cast<QnAbstractMediaData>(data))
                addQueueSizeUnsafe(- (qint64) media->dataSize());
        }
    }

    return true;
}

bool QnServerStreamRecorder::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (motion)
        m_serverModule->motionHelper()->saveToArchive(motion);
    return true;
}

QnScheduleTask QnServerStreamRecorder::currentScheduleTask() const
{
    QnMutexLocker lock( &m_scheduleMutex );
    return static_cast<QnScheduleTask>(m_currentScheduleTask);
}

void QnServerStreamRecorder::initIoContext(
    const QnStorageResourcePtr& storage,
    const QString& url,
    AVIOContext** context)
{
    Q_ASSERT(context);
    auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
    if (fileStorage)
    {
        QIODevice* ioDevice = fileStorage->open(
            url,
            QIODevice::WriteOnly,
            getBufferSize());
        if (ioDevice == 0)
        {
            *context = nullptr;
            return;
        }
        *context = QnFfmpegHelper::createFfmpegIOContext(ioDevice);
        if (!*context)
            return;
        fileCreated((uintptr_t)ioDevice);
    }
    else
        QnStreamRecorder::initIoContext(storage, url, context);
}

void QnServerStreamRecorder::fileCreated(uintptr_t filePtr) const
{
    connect(
        (QBufferedFile*)filePtr,
        &QBufferedFile::seekDetected,
        &m_serverModule->recordingManager()->getBufferManager(),
        &WriteBufferMultiplierManager::at_seekDetected,
        Qt::DirectConnection);
    connect(
        (QBufferedFile*)filePtr,
        &QBufferedFile::fileClosed,
        &m_serverModule->recordingManager()->getBufferManager(),
        &WriteBufferMultiplierManager::at_fileClosed,
        Qt::DirectConnection);
    m_serverModule->recordingManager()->getBufferManager().setFilePtr(
        filePtr,
        m_catalog,
        m_resource->getId());
}

int QnServerStreamRecorder::getBufferSize() const
{
    return m_serverModule->recordingManager()->getBufferManager().getSizeForCam(
        m_catalog,
        m_resource->getId());
}

void QnServerStreamRecorder::updateStreamParams()
{
    const nx::vms::server::resource::Camera* camera = dynamic_cast<const nx::vms::server::resource::Camera*>(m_resource.data());
    if (m_mediaProvider)
    {
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
        if (m_catalog == QnServer::HiQualityCatalog)
        {
            QnLiveStreamParams params;
            if (m_currentScheduleTask.recordingType != Qn::RecordingType::never && camera->isLicenseUsed())
            {
                params.fps = m_currentScheduleTask.fps;
                params.quality = m_currentScheduleTask.streamQuality;
                params.bitrateKbps = m_currentScheduleTask.bitrateKbps;
            }
            else {
                NX_ASSERT(camera);

                params.fps = camera->getMaxFps();
                params.quality = Qn::StreamQuality::highest;
                params.bitrateKbps = 0;
            }
            liveProvider->setPrimaryStreamParams(params);
        }
        liveProvider->setCameraControlDisabled(camera->isCameraControlDisabled());
    }
}

bool QnServerStreamRecorder::isMotionRec(Qn::RecordingType recType) const
{
    const QnSecurityCamResource* camera = static_cast<const nx::vms::server::resource::Camera*>(m_resource.data());
    return recType == Qn::RecordingType::motionOnly ||
           (m_catalog == QnServer::HiQualityCatalog && recType == Qn::RecordingType::motionAndLow && camera->hasDualStreaming());
}

void QnServerStreamRecorder::beforeProcessData(const QnConstAbstractMediaDataPtr& media)
{
    setLastMediaTime(media->timestamp);

    NX_ASSERT(m_dualStreamingHelper, "Dual streaming helper must be defined!");
    QnConstMetaDataV1Ptr metaData = std::dynamic_pointer_cast<const QnMetaDataV1>(media);
    if (metaData) {
        m_dualStreamingHelper->onMotion(metaData.get());
        qint64 motionTime = m_dualStreamingHelper->getLastMotionTime();
        if (!metaData->isEmpty()) {
            updateMotionStateInternal(true, metaData->timestamp, metaData);
            m_lastMotionTimeUsec = motionTime;
        }
        return;
    }

    ScheduleTaskWithThresholds task;
    {
        QnMutexLocker lock( &m_scheduleMutex );
        task = m_currentScheduleTask;
    }
    bool isRecording = task.recordingType != Qn::RecordingType::never
        && m_serverModule->normalStorageManager()->isWritableStoragesAvailable();
    if (!m_resource->hasFlags(Qn::foreigner)) {
        if (isRecording) {
            if(m_resource->getStatus() == Qn::Online)
                m_resource->setStatus(Qn::Recording);
        }
        else {
            if(m_resource->getStatus() == Qn::Recording)
                m_resource->setStatus(Qn::Online);
        }
    }

    if (!isMotionRec(task.recordingType))
        return;

    qint64 motionTime = m_dualStreamingHelper->getLastMotionTime();
    if (motionTime == (qint64)AV_NOPTS_VALUE)
    {
        // No more motion, set prebuffer again.
        setPrebufferingUsec(task.recordBeforeSec * 1000000ll);
    }
    else
    {
        m_lastMotionTimeUsec = motionTime;
        setPrebufferingUsec(0); // motion in progress, flush prebuffer
    }

    return;
}

void QnServerStreamRecorder::updateMotionStateInternal(bool value, qint64 timestamp, const QnConstMetaDataV1Ptr& metaData)
{
    if (m_lastMotionState == value && !value)
        return;
    m_lastMotionState = value;
    emit motionDetected(getResource(), m_lastMotionState, timestamp, metaData);
}

void QnServerStreamRecorder::updateContainerMetadata(QnAviArchiveMetadata* metadata) const
{
    using namespace nx::vms::server;
    metadata->version = QnAviArchiveMetadata::kIntegrityCheckVersion;
    metadata->integrityHash =
        IntegrityHashHelper::generateIntegrityHash(QByteArray::number(m_startDateTimeUs / 1000));
}

bool QnServerStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& media)
{
    qint64 afterThreshold = 5 * 1000000ll;
    Qn::RecordingType recType = m_currentScheduleTask.recordingType;
    if (recType == Qn::RecordingType::motionOnly || recType == Qn::RecordingType::motionAndLow)
        afterThreshold = m_currentScheduleTask.recordAfterSec * 1000000ll;
    bool isMotionContinue = m_lastMotionTimeUsec != (qint64)AV_NOPTS_VALUE && media->timestamp < m_lastMotionTimeUsec + afterThreshold;
    if (!isMotionContinue)
    {
        if (m_endDateTimeUs == (qint64)AV_NOPTS_VALUE || media->timestamp - m_endDateTimeUs < MAX_FRAME_DURATION_MS*1000)
            updateMotionStateInternal(false, media->timestamp, QnMetaDataV1Ptr());
        else
            updateMotionStateInternal(false, m_endDateTimeUs + MIN_FRAME_DURATION_USEC, QnMetaDataV1Ptr());
    }
    QnScheduleTask task = currentScheduleTask();

    const auto camera = static_cast<const nx::vms::server::resource::Camera*>(m_resource.data());
    const QnMetaDataV1* metaData = dynamic_cast<const QnMetaDataV1*>(media.get());

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

    if (task.recordingType == Qn::RecordingType::always)
        return true;
    else if (task.recordingType == Qn::RecordingType::motionAndLow && (m_catalog == QnServer::LowQualityCatalog || !camera->hasDualStreaming()))
        return true;
    else if (task.recordingType == Qn::RecordingType::never)
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
    if (!isMotionContinue && m_endDateTimeUs != (qint64)AV_NOPTS_VALUE)
    {
        if (media->timestamp - m_endDateTimeUs < MAX_FRAME_DURATION_MS*1000)
            m_endDateTimeUs = media->timestamp;
        else
            m_endDateTimeUs += MIN_FRAME_DURATION_USEC;
        close();
    }
    return isMotionContinue;
}

int QnServerStreamRecorder::getFpsForValue(int fps)
{
    const nx::vms::server::resource::Camera* camera = dynamic_cast<const nx::vms::server::resource::Camera*>(m_resource.data());
    if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing)
    {
        if (m_catalog == QnServer::HiQualityCatalog)
            return fps ? qMin(fps, camera->getMaxFps()-2) : camera->getMaxFps()-2;
        else
            return QnLiveStreamParams::kFpsNotInitialized;
    }
    else
    {
        if (m_catalog == QnServer::HiQualityCatalog)
            return fps ? fps : camera->getMaxFps();
        else
            return QnLiveStreamParams::kFpsNotInitialized;
    }
}

void QnServerStreamRecorder::startForcedRecording(Qn::StreamQuality quality, int fps,
    int beforeThreshold, int afterThreshold, int maxDurationSec)
{
    m_forcedScheduleRecordTask.startTime = 0;
    m_forcedScheduleRecordTask.endTime = 24*3600*7;
    m_forcedScheduleRecordTask.recordBeforeSec = beforeThreshold;
    m_forcedScheduleRecordTask.recordAfterSec = afterThreshold;
    m_forcedScheduleRecordTask.fps = getFpsForValue(fps);
    if (maxDurationSec)
    {
        QDateTime dt = qnSyncTime->currentDateTime();
        int currentWeekSeconds = (dt.date().dayOfWeek()-1)*3600*24 + dt.time().hour()*3600 + dt.time().minute()*60 +  dt.time().second();
        m_forcedScheduleRecordTask.endTime = currentWeekSeconds + maxDurationSec;
    }
    m_forcedScheduleRecordTask.recordingType = Qn::RecordingType::always;
    m_forcedScheduleRecordTask.streamQuality = quality;

    m_forcedScheduleRecordTimer.restart();
    m_forcedScheduleRecordDurationMs = maxDurationSec * 1000;

    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());
}

void QnServerStreamRecorder::stopForcedRecording()
{
    m_forcedScheduleRecordTask.endTime = 0;
    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());
}

void QnServerStreamRecorder::updateRecordingType(const ScheduleTaskWithThresholds& scheduleTask)
{
    if (!isMotionRec(scheduleTask.recordingType))
    {
        // switch from motion to non-motion recording
        const QnResourcePtr& res = getResource();
        const QnSecurityCamResource* camRes = dynamic_cast<const QnSecurityCamResource*>(res.data());
        bool isNoRec = scheduleTask.recordingType == Qn::RecordingType::never;
        bool usedInRecordAction = camRes && camRes->isRecordingEventAttached();

        // prebuffer 1 usec if camera used in recording action (for keeping last GOP)
        int prebuffer = usedInRecordAction && isNoRec && !camRes->isIOModule() ? 1 : 0;
        setPrebufferingUsec(prebuffer);
    }
    else if (getPrebufferingUsec() != 0 || !isMotionRec(m_currentScheduleTask.recordingType)) {
        // do not change prebuffer if previous recording is motion and motion in progress
        setPrebufferingUsec(scheduleTask.recordBeforeSec * 1000000ll);
    }
    m_currentScheduleTask = scheduleTask;
}

void QnServerStreamRecorder::setSpecialRecordingMode(const ScheduleTaskWithThresholds& task)
{
    updateRecordingType(task);
    updateStreamParams();
    m_lastSchedulePeriod.clear();
}

bool QnServerStreamRecorder::isPanicMode() const
{
    const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
    return boost::algorithm::any_of(onlineServers
        , [](const QnMediaServerResourcePtr& server)
    {
        return (server->getPanicMode() != Qn::PM_None);
    });
}

void QnServerStreamRecorder::updateScheduleInfo(qint64 timeMs)
{
    QnMutexLocker lock( &m_scheduleMutex );

    if (isPanicMode())
    {
        if (!m_usedPanicMode)
        {
            setSpecialRecordingMode(m_panicScheduleRecordTask);
            m_usedPanicMode = true;
        }
        return;
    }
    else if (!m_forcedScheduleRecordTask.isEmpty())
    {
        if (!m_usedSpecialRecordingMode)
        {
            setSpecialRecordingMode(m_forcedScheduleRecordTask);
            m_usedSpecialRecordingMode = true;
        }
        bool isExpired = m_forcedScheduleRecordDurationMs > 0
             && m_forcedScheduleRecordTimer.hasExpired(m_forcedScheduleRecordDurationMs);
        if (isExpired)
            stopForcedRecording();
        else
            return;
    }

    m_usedSpecialRecordingMode = m_usedPanicMode = false;
    const ScheduleTaskWithThresholds noRecordTask;

    if (!m_schedule.isEmpty())
    {
        //bool isEmptyPeriod = m_skipDataToTime != AV_NOPTS_VALUE && timeMs < m_skipDataToTime;
        if (!m_lastSchedulePeriod.contains(timeMs))
        {
            // find new schedule
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timeMs);
            int scheduleTimeMs = (dt.date().dayOfWeek()-1)*3600*24 + dt.time().hour()*3600+dt.time().minute()*60+dt.time().second();
            scheduleTimeMs *= 1000;

            auto itr = std::upper_bound(m_schedule.begin(), m_schedule.end(), scheduleTimeMs,
                [](const int ms, const QnScheduleTask& task)
                {
                    return ms < taskStartTimeMs(task);
                });
            if (itr > m_schedule.begin())
                --itr;

            if (scheduleTaskContainsWeekTimeMs(*itr, scheduleTimeMs))
            {
                const auto camera =
                    static_cast<const nx::vms::server::resource::Camera*>(m_resource.data());

                ScheduleTaskWithThresholds task(*itr);
                task.recordBeforeSec = camera->recordBeforeMotionSec();
                task.recordAfterSec = camera->recordAfterMotionSec();

                updateRecordingType(task);
                updateStreamParams();
            }
            else {
                updateRecordingType(noRecordTask);
            }
            static const qint64 SCHEDULE_AGGREGATION = 1000*60*15;
            m_lastSchedulePeriod = QnTimePeriod(qFloor(timeMs, SCHEDULE_AGGREGATION)-MAX_FRAME_DURATION_MS, SCHEDULE_AGGREGATION+MAX_FRAME_DURATION_MS); // check period each 15 min
        }
    }
    else {
        updateRecordingType(noRecordTask);
    }
}

void QnServerStreamRecorder::addQueueSizeUnsafe(qint64 value)
{
    m_queuedSize += value;
    m_totalQueueSize += value;
}

bool QnServerStreamRecorder::processData(const QnAbstractDataPacketPtr& data)
{
    const QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!media)
        return true; //< skip data

    {
        QnMutexLocker lock( &m_queueSizeMutex );
        addQueueSizeUnsafe(-((qint64) media->dataSize()));
    }

    // For empty schedule, we record all the time.
    beforeProcessData(media);

    return QnStreamRecorder::processData(data);
}

void QnServerStreamRecorder::updateCamera(const QnSecurityCamResourcePtr& cameraRes)
{
    QnMutexLocker lock( &m_scheduleMutex );
    m_schedule = cameraRes->getScheduleTasks();
    NX_ASSERT(m_dualStreamingHelper, "DualStreaming helper must be defined!");
    m_lastSchedulePeriod.clear();
    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());

    if (m_mediaProvider)
    {
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
        liveProvider->updateSoftwareMotion();
    }
}

bool QnServerStreamRecorder::isRedundantSyncOn() const
{
    auto mediaServer = commonModule()->currentServer();
    NX_ASSERT(mediaServer);

    if (mediaServer->getBackupSchedule().backupType != nx::vms::api::BackupType::realtime)
        return false;

    auto cam = m_resource.dynamicCast<QnSecurityCamResource>();
    NX_ASSERT(cam);

    Qn::CameraBackupQualities cameraBackupQualities = cam->getActualBackupQualities();

    if (m_catalog == QnServer::HiQualityCatalog)
        return cameraBackupQualities.testFlag(Qn::CameraBackupQuality::CameraBackup_HighQuality);

    NX_ASSERT(m_catalog == QnServer::LowQualityCatalog, "Only two options are allowed");
    return cameraBackupQualities.testFlag(Qn::CameraBackupQuality::CameraBackup_LowQuality);
}

void QnServerStreamRecorder::getStoragesAndFileNames(QnAbstractMediaStreamDataProvider* provider)
{
    if (!m_fixedFileName)
    {
        QnMutexLocker lock(&m_queueSizeMutex);
        QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(m_resource);
        NX_ASSERT(netResource != 0, "Only network resources can be used with storage manager!");
        m_recordingContextVector.clear();

        auto normalStorage = m_serverModule->normalStorageManager()->getOptimalStorageRoot();
        QnStorageResourcePtr backupStorage;

        if (isRedundantSyncOn())
            backupStorage = m_serverModule->backupStorageManager()->getOptimalStorageRoot();

        if (normalStorage || backupStorage)
            setTruncateInterval(
                m_serverModule->settings().mediaFileDuration());

        if (normalStorage)
            m_recordingContextVector.emplace_back(
                m_serverModule->normalStorageManager()->getFileName(
                    m_startDateTimeUs/1000,
                    m_currentTimeZone,
                    netResource,
                    DeviceFileCatalog::prefixByCatalog(m_catalog),
                    normalStorage
                ),
                normalStorage
            );

        if (backupStorage)
            m_recordingContextVector.emplace_back(
                m_serverModule->backupStorageManager()->getFileName(
                    m_startDateTimeUs/1000,
                    m_currentTimeZone,
                    netResource,
                    DeviceFileCatalog::prefixByCatalog(m_catalog),
                    backupStorage
                ),
                backupStorage
            );
    }
}

void QnServerStreamRecorder::fileFinished(
    qint64 durationMs,
    const QString& fileName,
    QnAbstractMediaStreamDataProvider* provider,
    qint64 fileSize,
    qint64 startTimeMs)
{
    if (m_truncateInterval != 0)
    {
        m_serverModule->normalStorageManager()->fileFinished(
            durationMs,
            fileName,
            provider,
            fileSize,
            startTimeMs
        );

        m_serverModule->backupStorageManager()->fileFinished(
            durationMs,
            fileName,
            provider,
            fileSize,
            startTimeMs
        );
    }
};

void QnServerStreamRecorder::fileStarted(
    qint64 startTimeMs,
    int timeZone,
    const QString& fileName,
    QnAbstractMediaStreamDataProvider* provider,
    bool sideRecorder)
{
    if (m_truncateInterval > 0)
    {
        m_serverModule->normalStorageManager()->fileStarted(
            startTimeMs,
            timeZone,
            fileName,
            provider,
            sideRecorder
        );

        m_serverModule->backupStorageManager()->fileStarted(
            startTimeMs,
            timeZone,
            fileName,
            provider,
            sideRecorder
        );
    }
}

void QnServerStreamRecorder::beforeRun()
{
    ++m_totalRecorders;
}

void QnServerStreamRecorder::endOfRun()
{
    updateMotionStateInternal(false, m_lastMediaTime, QnMetaDataV1Ptr());

    QnStreamRecorder::endOfRun();
    if(m_resource->getStatus() == Qn::Recording)
        m_resource->setStatus(Qn::Online);

    {
        QnMutexLocker lock( &m_queueSizeMutex );
        addQueueSizeUnsafe(-m_queuedSize);
        m_dataQueue.clear();
        --m_totalRecorders;
        m_rebuildBlocked = false;
        DeviceFileCatalog::rebuildResume(this);
    }
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
    return m_forcedScheduleRecordTask.recordAfterSec;
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
    if (m_recentlyMotion.size() == kMotionPrebufferSize)
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
    const nx::vms::server::resource::Camera* camera = dynamic_cast<nx::vms::server::resource::Camera*>(m_resource.data());
    return camera->isLicenseUsed();
}

bool QnServerStreamRecorder::forceDefaultContext(const QnConstAbstractMediaDataPtr& frame) const
{
    // Do not put codec context to the container level if video stream has built-in context.
    // It is CPU optimization for server recording. Ffmpeg works faster at this mode.
    return mediaHasBuiltinContext(frame);
}

bool QnServerStreamRecorder::mediaHasBuiltinContext(const QnConstAbstractMediaDataPtr& frame) const
{
    auto videoFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(frame);

    if (!videoFrame)
        return false;

    auto codecId = videoFrame->compressionType;
    using namespace nx::media_utils;
    switch (codecId)
    {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_HEVC:
        {
            std::vector<std::pair<const quint8*, size_t>> nalUnits;
            readNALUsFromAnnexBStream(videoFrame, &nalUnits);

            for (const std::pair<const quint8*, size_t>& nalu: nalUnits)
            {
                if (codecId == AV_CODEC_ID_H264)
                {
                    auto nalUnitType = *nalu.first & 0x1f;
                    if (nalUnitType == NALUnitType::nuSPS)
                        return true;
                }
                else if (codecId == AV_CODEC_ID_HEVC)
                {
                    hevc::NalUnitHeader header;
                    header.decode(nalu.first, nalu.second);
                    if (hevc::isParameterSet(header.unitType))
                        return true;
                }
            }
        }
        default:
            return false;
    }
}
