#include "recording_manager.h"

#include <core/resource_management/resource_properties.h>
#include <core/resource/resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include "api/app_server_connection.h"
#include "api/global_settings.h"
#include "recording/stream_recorder.h"
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include "camera/camera_pool.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"
#include "nx/streaming/archive_stream_reader.h"
#include "camera/video_camera.h"
#include "core/misc/schedule_task.h"
#include <nx/utils/log/log.h>
#include "utils/common/synctime.h"
#include <nx/vms/event/rule.h>
#include <nx/vms/server/event/rule_processor.h>
#include <nx/vms/server/event/event_connector.h>
#include <media_server/serverutil.h>
#include "licensing/license.h"
#include "mutex/camera_data_handler.h"
#include "mutex/distributed_mutex_manager.h"
#include "common/common_module.h"
#include "api/runtime_info_manager.h"
#include "utils/license_usage_helper.h"
#include "media_server/settings.h"
#include "core/resource/camera_user_attribute_pool.h"
#include "mutex/camera_data_handler.h"
#include <mutex/distributed_mutex_manager.h>

#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <media_server/media_server_module.h>
#include <motion/motion_helper.h>

static const QString LICENSE_OVERFLOW_LOCK_NAME(lit("__LICENSE_OVERFLOW__"));

using namespace nx::vms::server;

RecorderData::~RecorderData()
{
    if (recorder)
        recorder->stop();
    if (reorderingProvider)
    {
        reorderingProvider->stop();
        reorderingProvider->removeDataProcessor(recorder.get());
    }
}

QnRecordingManager::QnRecordingManager(
    QnMediaServerModule* serverModule,
    ec2::QnDistributedMutexManager* mutexManager)
:
    nx::vms::server::ServerModuleAware(serverModule),
    m_mutex(QnMutex::Recursive),
    m_mutexManager(mutexManager)
{
    m_tooManyRecordingCnt = 0;
    m_licenseMutex = nullptr;
    connect(
        this, &QnRecordingManager::recordingDisabled,
        serverModule->eventConnector(), &nx::vms::server::event::EventConnector::at_licenseIssueEvent);
    m_recordingStopTimeMs = serverModule->settings().forceStopRecordingTime();
    m_recordingStopTimeMs *= 1000;

    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &QnRecordingManager::onNewResource, Qt::QueuedConnection);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, &QnRecordingManager::onRemoveResource, Qt::QueuedConnection);
    connect(&m_scheduleWatchingTimer, &QTimer::timeout, this, &QnRecordingManager::onTimer);
    connect(&m_licenseTimer, &QTimer::timeout, this, &QnRecordingManager::at_checkLicenses);
}

QnRecordingManager::~QnRecordingManager()
{
    QnMutexLocker lock(&m_resourceConnectionMutex);
    // We shouldn't receive any new recording orders if the destructor is called
    for (auto& camera: resourcePool()->getResources<QnVirtualCameraResource>())
        camera->disconnect(camera.data(), nullptr, this, nullptr);

    stop();
}

void QnRecordingManager::updateRuntimeInfoAfterLicenseOverflowTransaction(qint64 prematureLicenseExperationDate)
{
    QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
    if (localInfo.data.prematureLicenseExperationDate != prematureLicenseExperationDate)
    {
        localInfo.data.prematureLicenseExperationDate = prematureLicenseExperationDate;
        runtimeInfoManager()->updateLocalItem(localInfo);
    }
}

void QnRecordingManager::start()
{
    m_scheduleWatchingTimer.start(1000);
    m_licenseTimer.start(1000 * 60);
    QThread::start();
}

void QnRecordingManager::beforeDeleteRecorder(const Recorders& recorders)
{
    if(recorders.recorderHiRes)
        recorders.recorderHiRes->recorder->pleaseStop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->recorder->pleaseStop();
}

void QnRecordingManager::stop()
{
    exit();
    wait(); // stop QT event loop

    QnMutexLocker lock( &m_mutex );

    for(const auto& recorders: m_recordMap)
        beforeDeleteRecorder(recorders.second);

    m_recordMap.clear();
    m_scheduleWatchingTimer.stop();

    onTimer();
    m_delayedStop.clear();
}

std::optional<std::chrono::microseconds> QnRecordingManager::recorderFileDuration(
    const QnResourcePtr& res,
    QnServer::ChunksCatalog catalog) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_recordMap.find(res);
    if (itr == m_recordMap.end())
        return std::nullopt;
    const auto& recorderData = (catalog == QnServer::ChunksCatalog::HiQualityCatalog)
        ? itr->second.recorderHiRes : itr->second.recorderLowRes;
    if (!recorderData)
        return std::nullopt;
    return std::chrono::microseconds(recorderData->recorder->duration());
}

std::unique_ptr<RecorderData> QnRecordingManager::createRecorder(
    const QnResourcePtr &res, const QnAbstractMediaStreamDataProviderPtr& reader,
    QnServer::ChunksCatalog catalog, const QnDualStreamingHelperPtr& dualStreamingHelper)
{
    auto recorder = std::make_unique<QnServerStreamRecorder>(
        serverModule(), res, catalog, reader.data());
    recorder->setDualStreamingHelper(dualStreamingHelper);
    recorder->setTruncateInterval(RECORDING_CHUNK_LEN);

    auto reorderingDataProvider = std::make_unique<PutInOrderDataProvider>(
            reader,
            std::chrono::milliseconds(0) /*minSize*/,
            std::chrono::seconds(5) /*maxSize*/,
            std::chrono::seconds(0)  /*initialSize*/,
            PutInOrderDataProvider::BufferingPolicy::increaseOnly);
    reorderingDataProvider->addDataProcessor(recorder.get());

    return std::make_unique<RecorderData>(std::move(recorder), std::move(reorderingDataProvider));
}

bool QnRecordingManager::isResourceDisabled(const QnResourcePtr& res) const
{
    if (res->hasFlags(Qn::foreigner))
        return true;

    const QnVirtualCameraResource* cameraRes = dynamic_cast<const QnVirtualCameraResource*>(res.data());
    return  cameraRes && !cameraRes->isLicenseUsed();
}

bool QnRecordingManager::startForcedRecording(
    const QnSecurityCamResourcePtr& camRes,
    Qn::StreamQuality quality,
    int fps,
    int beforeThresholdSec,
    int afterThresholdSec,
    int maxDurationSec)
{
    updateCamera(camRes); // ensure recorders are created
    auto camera = videoCameraPool()->getVideoCamera(camRes);
    if (!camera)
        return false;

    QnMutexLocker lock( &m_mutex );

    auto itrRec = m_recordMap.find(camRes);
    if (itrRec == m_recordMap.end())
        return false;

    m_delayedStop.remove(camRes);

    // update current schedule task
    Recorders& recorders = itrRec->second;
    if (recorders.recorderHiRes)
    {
        recorders.recorderHiRes->recorder->startForcedRecording(
            quality, fps, beforeThresholdSec, afterThresholdSec, maxDurationSec);
    }
    if (recorders.recorderLowRes)
    {
        recorders.recorderLowRes->recorder->startForcedRecording(
            quality, fps, beforeThresholdSec, afterThresholdSec, maxDurationSec);
    }

    // start recorder threads
    startOrStopRecording(camRes, camera, recorders);

    // return true if recording started. if camera is not accessible e.t.c return false
    return true;
}

bool QnRecordingManager::stopForcedRecording(const QnSecurityCamResourcePtr& camRes, bool afterThresholdCheck)
{
    updateCamera(camRes); // ensure recorders are created
    auto camera = videoCameraPool()->getVideoCamera(camRes);
    if (!camera)
        return false;

    QnMutexLocker lock( &m_mutex );

    auto itrRec = m_recordMap.find(camRes);
    if (itrRec == m_recordMap.end())
        return false;

    Recorders& recorders = itrRec->second;
    if (!recorders.recorderHiRes && !recorders.recorderLowRes)
        return false;

    int afterThreshold = recorders.recorderHiRes
        ? recorders.recorderHiRes->recorder->getFRAfterThreshold()
        : recorders.recorderLowRes->recorder->getFRAfterThreshold();
    if (afterThreshold > 0 && afterThresholdCheck)
    {
        m_delayedStop.insert(camRes, qnSyncTime->currentUSecsSinceEpoch() + afterThreshold*1000000ll);
        return true;
    }
    m_delayedStop.remove(camRes);

    // update current schedule task
    if (recorders.recorderHiRes)
        recorders.recorderHiRes->recorder->stopForcedRecording();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->recorder->stopForcedRecording();

    // start recorder threads
    startOrStopRecording(camRes, camera, recorders);
    return true;
}

void QnRecordingManager::startRecording(
    const QnVideoCameraPtr& camera,
    std::unique_ptr<RecorderData>& recorder,
    const QnLiveStreamProviderPtr& provider,
    QnServer::ChunksCatalog quality,
    const QSharedPointer<QnDualStreamingHelper>& dualStreamingHelper)
{
    const auto id = camera->resource()->getUniqueId();
    if (!provider)
    {
        NX_WARNING(this, "Can not start recording for camera %1 because of no data provider", id);
        return;
    }
    if (!recorder)
    {
        recorder = createRecorder(camera->resource(), provider, quality, dualStreamingHelper);
        if (auto camRes = camera->resource().dynamicCast<QnSecurityCamResource>())
            recorder->recorder->updateCamera(camRes);
    }

    if (!recorder->recorder->isRunning())
    {
        NX_INFO(this, "Recording started for camera %1", id);
        // Reset eof time from recorder
        recorder->recorder->setProgressBounds(AV_NOPTS_VALUE, AV_NOPTS_VALUE);
        camera->inUse(recorder->recorder.get());
        recorder->recorder->start();
        provider->startIfNotRunning();
    }
}

bool QnRecordingManager::stopRecording(
    const QnVideoCameraPtr& camera,
    std::unique_ptr<RecorderData>& recorder,
    const QnLiveStreamProviderPtr& provider)
{
    if (!recorder)
        return false;

    const auto id = camera->resource()->getUniqueId();
    // Recording thread can be stopped outside, not by RecordingManager
    // So, remove cameraUse anyway
    camera->notInUse(recorder->recorder.get());
    if (recorder->recorder->isRunning())
    {
        NX_INFO(this, "Recording is stopped for camera %1", id);
        const auto lastTimestamp = recorder->reorderingProvider->flush();
        if (lastTimestamp) //< Record reordering buffer before stop.
            recorder->recorder->setProgressBounds(AV_NOPTS_VALUE, lastTimestamp->count());
        else
            recorder->recorder->pleaseStop();
        return true;
    }
    return false;
}

void QnRecordingManager::startOrStopRecording(
    const QnResourcePtr& res, const QnVideoCameraPtr& camera, Recorders& recorders)
{
    QnSecurityCamResourcePtr cameraRes = res.dynamicCast<QnSecurityCamResource>();
    bool needRecordCamera =
        !isResourceDisabled(res) &&
        !cameraRes->isDtsBased() &&
        !cameraRes->needsToChangeDefaultPassword();

    QnLiveStreamProviderPtr providerHi = camera->getLiveReader(QnServer::HiQualityCatalog, needRecordCamera);
    QnLiveStreamProviderPtr providerLow = camera->getLiveReader(QnServer::LowQualityCatalog, needRecordCamera);

    NX_VERBOSE(this, "Need recording camera %1 is %2", camera->resource()->getUniqueId(), needRecordCamera);

    if (needRecordCamera && res->getStatus() != Qn::Offline)
    {
        if (!cameraRes->isInitialized())
        {
            NX_VERBOSE(this, "Camera %1 is need to be recorded, but not initialized yet", camera->resource()->getUniqueId());
            return; // wait for initialization
        }

        if (providerHi)
            startRecording(camera, recorders.recorderHiRes, providerHi,
                QnServer::HiQualityCatalog, recorders.dualStreamingHelper);

        if (providerLow)
        {
            const auto currentFps = recorders.recorderHiRes
                ? recorders.recorderHiRes->recorder->currentScheduleTask().fps : 0;

            // second stream should run if camera do not share fps or at least MIN_SECONDARY_FPS frames left for second stream
            bool runSecondStream = cameraRes->isEnoughFpsToRunSecondStream(currentFps) &&
                                    cameraRes->hasDualStreaming() && providerLow;
            if (runSecondStream)
                startRecording(camera, recorders.recorderLowRes, providerLow,
                    QnServer::LowQualityCatalog, recorders.dualStreamingHelper);
            else
                stopRecording(camera, recorders.recorderLowRes, providerLow);
        }
    }
    else
    {
        bool threadStopped = stopRecording(camera, recorders.recorderHiRes, providerHi);
        threadStopped |= stopRecording(camera, recorders.recorderLowRes, providerLow);

        if (!res->hasFlags(Qn::foreigner) && !threadStopped && res->getStatus() == Qn::Recording)
                res->setStatus(Qn::Online); // may be recording thread was not run, so reset status to online
    }

    //doing anyway to stop internal cache, etc...
    camera->stopIfNoActivity();
}

void QnRecordingManager::updateCamera(const QnSecurityCamResourcePtr& cameraRes)
{
    if (serverModule()->commonModule()->isNeedToStop())
        return;

    if (!cameraRes)
        return;
    if (cameraRes->hasFlags(Qn::foreigner) && m_recordMap.count(cameraRes) == 0)
        return;

    if (cameraRes->hasFlags(Qn::desktop_camera))
        return;

    auto camera = videoCameraPool()->getVideoCamera(cameraRes);
    if (!camera)
        return;

    m_mutex.lock();
    Recorders& recorders = m_recordMap[cameraRes];
    m_mutex.unlock(); // todo: refactor it. I've just keep current logic

    if (!recorders.dualStreamingHelper)
        recorders.dualStreamingHelper = QnDualStreamingHelperPtr(new QnDualStreamingHelper());

    if (cameraRes->isInitialized())
    {
        if (recorders.recorderHiRes)
            recorders.recorderHiRes->recorder->updateCamera(cameraRes);
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->recorder->updateCamera(cameraRes);
    }

    startOrStopRecording(cameraRes, camera, recorders);
}

void QnRecordingManager::at_camera_resourceChanged(const QnResourcePtr& /*resource*/)
{
    QnVirtualCameraResource* cameraPtr = dynamic_cast<QnVirtualCameraResource*>(sender());
    if( !cameraPtr )
        return;

    const QnVirtualCameraResourcePtr& camera = cameraPtr->toSharedPointer().staticCast<QnVirtualCameraResource>();
    const bool ownResource = !camera->hasFlags(Qn::foreigner);
    if (ownResource && !camera->isInitialized())
    {
        NX_VERBOSE(this, "Trying to init resource [%1] after status change to [%2]",
            camera, camera->getStatus());
        camera->initAsync(false);
    }

    updateCamera(camera);

    // no need mutex here because of readOnly access from other threads and m_recordMap modified only from current thread
    //QnMutexLocker lock( &m_mutex );

    auto itr = m_recordMap.find(camera); // && m_recordMap.value(camera).recorderHiRes->isRunning();
    if (itr != m_recordMap.end() && ownResource)
    {
        const auto& value = itr->second;
        if (value.recorderHiRes && value.recorderHiRes->recorder->isAudioPresent() != camera->isAudioEnabled())
            value.recorderHiRes->recorder->setNeedReopen();
        if (value.recorderLowRes && value.recorderLowRes->recorder->isAudioPresent() != camera->isAudioEnabled())
            value.recorderLowRes->recorder->setNeedReopen();
    }
}

void QnRecordingManager::at_camera_initializationChanged(const QnResourcePtr &resource)
{
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (camera && camera->isInitialized())
        updateCamera(camera);
}

void QnRecordingManager::onNewResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock(&m_resourceConnectionMutex);
    if (serverModule()->commonModule()->isNeedToStop())
        return;

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(resource);
    Q_ASSERT(!videoCameraPool()->getVideoCamera(resource));
    if (videoCameraPool()->getVideoCamera(resource))
        NX_WARNING(this, lit("%1: VideoCamera for this resource %2 already exists")
                .arg(Q_FUNC_INFO)
                .arg(resource->getUrl()));
    videoCameraPool()->addVideoCamera(resource);
    if (camera)
    {
        connect(camera.data(), &QnResource::initializedChanged, this, &QnRecordingManager::at_camera_initializationChanged);
        connect(camera.data(), &QnResource::resourceChanged,    this, &QnRecordingManager::at_camera_resourceChanged);
        connect(camera.data(), &QnVirtualCameraResource::recordingActionChanged,    this, &QnRecordingManager::at_camera_resourceChanged);
        updateCamera(camera);
    }

    QnMediaServerResourcePtr server = qSharedPointerDynamicCast<QnMediaServerResource>(resource);
    if (server) {
        connect(server.data(), &QnResource::propertyChanged,  this, &QnRecordingManager::at_serverPropertyChanged);
    }
}

void QnRecordingManager::at_serverPropertyChanged(const QnResourcePtr &, const QString &key)
{
    if (key == QnMediaResource::panicRecordingKey())
    {
        NX_DEBUG(this, "Panic mode has changed, update camera");
        for (const auto& [camera, recorder]: m_recordMap)
            updateCamera(camera.dynamicCast<QnSecurityCamResource>());
    }
}

void QnRecordingManager::onRemoveResource(const QnResourcePtr &resource)
{
    QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (physicalStorage) {
        serverModule()->normalStorageManager()->removeStorage(physicalStorage);
        serverModule()->backupStorageManager()->removeStorage(physicalStorage);
        return;
    }

    {
        Recorders recorders;
        {
            QnMutexLocker lock( &m_mutex );
            auto itr = m_recordMap.find(resource);
            if (itr == m_recordMap.end())
                return;
            recorders.recorderHiRes = std::move(itr->second.recorderHiRes);
            recorders.recorderLowRes = std::move(itr->second.recorderLowRes);
            m_recordMap.erase(resource);
        }
        beforeDeleteRecorder(recorders);
    }

    videoCameraPool()->removeVideoCamera(resource);
    serverModule()->motionHelper()->remove(resource);
}

bool QnRecordingManager::isCameraRecoring(const QnResourcePtr& camera) const
{
    QnMutexLocker lock( &m_mutex );
    auto itr = m_recordMap.find(camera);
    if (itr == m_recordMap.end())
        return false;
    return (itr->second.recorderHiRes && itr->second.recorderHiRes->recorder->isRunning()) ||
           (itr->second.recorderLowRes && itr->second.recorderLowRes->recorder->isRunning());
}

void QnRecordingManager::onTimer()
{
    qint64 time = qnSyncTime->currentMSecsSinceEpoch();

    // Mutex is not required here because of m_recordMap used in readOnly mode here and m_recordMap modified from this thread only (from other private slot)
    //QnMutexLocker lock( &m_mutex );

    for (auto itrRec = m_recordMap.begin(); itrRec != m_recordMap.end(); ++itrRec)
    {
        if (!resourcePool()->getResourceById(itrRec->first->getId()))
            continue; //< resource just deleted. will be removed from m_recordMap soon
        auto camera = videoCameraPool()->getVideoCamera(itrRec->first);

        Recorders& recorders = itrRec->second;

        //if (!recorders.recorderHiRes && !recorders.recorderLowRes)
        //    continue; // no recorders are created now

        if (recorders.recorderHiRes)
            recorders.recorderHiRes->recorder->updateScheduleInfo(time);
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->recorder->updateScheduleInfo(time);

        startOrStopRecording(itrRec->first, camera, recorders);
    }

    QMap<QnSecurityCamResourcePtr, qint64> stopList = m_delayedStop;
    for (QMap<QnSecurityCamResourcePtr, qint64>::iterator itrDelayedStop = stopList.begin(); itrDelayedStop != stopList.end(); ++itrDelayedStop)
    {
        qint64 stopTime = itrDelayedStop.value();
        if (stopTime <= time*1000ll)
            stopForcedRecording(itrDelayedStop.key(), false);
    }
    videoCameraPool()->updateActivity();

}

QnVirtualCameraResourceList QnRecordingManager::getLocalControlledCameras() const
{
    // return own cameras + cameras from servers without DB (remote connected servers)
    QnVirtualCameraResourceList cameras = resourcePool()->getAllCameras(QnResourcePtr());
    QnVirtualCameraResourceList result;
    for(const QnVirtualCameraResourcePtr &camRes: cameras)
    {
        QnMediaServerResourcePtr mServer = camRes->getParentServer();
        if (!mServer)
            continue;
        if (mServer->getId() == moduleGUID()
            || mServer->getServerFlags().testFlag(nx::vms::api::SF_RemoteEC))
        {
            result << camRes;
        }
    }
    return result;
}

void QnRecordingManager::at_checkLicenses()
{
    if (m_licenseMutex)
        return;

    QnCamLicenseUsageHelper helper(serverModule()->commonModule());

    if (!helper.isValid())
    {
        if (++m_tooManyRecordingCnt < 5)
            return; // do not report license problem immediately. Server should wait several minutes, probably other servers will be available soon

        qint64 licenseOverflowTime = runtimeInfoManager()->localInfo().data.prematureLicenseExperationDate;
        if (licenseOverflowTime == 0) {
            licenseOverflowTime = qnSyncTime->currentMSecsSinceEpoch();
            auto errCode = ec2Connection()->getMiscManager(Qn::kSystemAccess)->markLicenseOverflowSync(true, licenseOverflowTime);
            if (errCode == ec2::ErrorCode::ok)
                updateRuntimeInfoAfterLicenseOverflowTransaction(licenseOverflowTime);
        }
        if (qnSyncTime->currentMSecsSinceEpoch() - licenseOverflowTime < m_recordingStopTimeMs)
            return; // not enough license, but timeout not reached yet

        // Too many licenses. check if server has own recording cameras and force to disable recording
        QnVirtualCameraResourceList ownCameras = getLocalControlledCameras();
        for(const QnVirtualCameraResourcePtr& camera: ownCameras)
        {
            if (helper.isOverflowForCamera(camera))
            {
                // found. remove recording from some of them

                if (m_mutexManager)
                {
                    m_licenseMutex = m_mutexManager->createMutex(LICENSE_OVERFLOW_LOCK_NAME);
                    connect(m_licenseMutex, &ec2::QnDistributedMutex::locked, this, &QnRecordingManager::at_licenseMutexLocked, Qt::QueuedConnection);
                    connect(m_licenseMutex, &ec2::QnDistributedMutex::lockTimeout, this, &QnRecordingManager::at_licenseMutexTimeout, Qt::QueuedConnection);
                    m_licenseMutex->lockAsync();
                }
                else
                {
                    disableLicensesIfNeed();
                }
                helper.invalidate();
                break;
            }
        }
    }
    else {
        qint64 licenseOverflowTime = runtimeInfoManager()->localInfo().data.prematureLicenseExperationDate;
        if (licenseOverflowTime)
        {
            auto errorCode = ec2Connection()->getMiscManager(Qn::kSystemAccess)->markLicenseOverflowSync(false, 0);
            if (errorCode == ec2::ErrorCode::ok)
                updateRuntimeInfoAfterLicenseOverflowTransaction(0);
        }

        m_tooManyRecordingCnt = 0;
    }
}

void QnRecordingManager::at_licenseMutexLocked()
{
    disableLicensesIfNeed();
    m_licenseMutex->unlock();
    m_licenseMutex->deleteLater();
    m_licenseMutex = nullptr;
}

void QnRecordingManager::disableLicensesIfNeed()
{
    QnCamLicenseUsageHelper helper(serverModule()->commonModule());

    QStringList disabledCameras;

    // Too many licenses. check if server has own recording cameras and force to disable recording
    const QnVirtualCameraResourceList& ownCameras = getLocalControlledCameras();
    for(const QnVirtualCameraResourcePtr& camera: ownCameras)
    {
        if (helper.isValid())
            break;

        if (helper.isOverflowForCamera(camera))
        {
            camera->setLicenseUsed(false);
            QList<QnUuid> idList;
            idList << camera->getId();

            QnCameraUserAttributesList userAttributes = cameraUserAttributesPool()->getAttributesList(idList);
            nx::vms::api::CameraAttributesDataList apiAttributes;
            ec2::fromResourceListToApi(userAttributes, apiAttributes);

            ec2::ErrorCode errCode =  ec2Connection()->getCameraManager(Qn::kSystemAccess)->saveUserAttributesSync(apiAttributes);
            if (errCode != ec2::ErrorCode::ok)
            {
                NX_WARNING(this, "Can't turn off recording for camera: %1. Error: %2",
                    camera->getUniqueId(), ec2::toString(errCode));
                camera->setLicenseUsed(true); // rollback
                continue;
            }
            camera->saveProperties();
            NX_INFO(this, "Turn off recording for camera %1 because there is not enough licenses",
                camera->getUniqueId());
            disabledCameras << camera->getId().toString();
            helper.invalidate();
        }
    }

    if (!disabledCameras.isEmpty()) {
        QnResourcePtr resource = resourcePool()->getResourceById(moduleGUID());
        // TODO: #gdm move (de)serializing of encoded reason params to common place
        emit recordingDisabled(resource, qnSyncTime->currentUSecsSinceEpoch(), nx::vms::api::EventReason::licenseRemoved, disabledCameras.join(L';'));
    }
}

void QnRecordingManager::at_licenseMutexTimeout()
{
    m_licenseMutex->deleteLater();
    m_licenseMutex = 0;
}

int WriteBufferMultiplierManager::getSizeForCam(
    QnServer::ChunksCatalog catalog,
    const QnUuid& resourceId)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_recToMult.find(Key(catalog, resourceId));
    if (it == m_recToMult.cend())
        return 0;
    else
        return it->second;
}

void WriteBufferMultiplierManager::setFilePtr(
    uintptr_t filePtr,
    QnServer::ChunksCatalog catalog,
    const QnUuid& resourceId)
{
    QnMutexLocker lock(&m_mutex);
    m_fileToRec[filePtr] = Key(catalog, resourceId);
}

void WriteBufferMultiplierManager::at_seekDetected(uintptr_t filePtr, int size)
{
    QnMutexLocker lock(&m_mutex);
    auto recIt = m_fileToRec.find(filePtr);
    if (recIt != m_fileToRec.cend())
        m_recToMult[recIt->second] = size;
}

void WriteBufferMultiplierManager::at_fileClosed(uintptr_t filePtr)
{
    QnMutexLocker lock(&m_mutex);
    m_fileToRec.erase(filePtr);
}
