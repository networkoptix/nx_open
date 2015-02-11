#include "recording_manager.h"
#include <core/resource_management/resource_properties.h>
#include <core/resource/resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource_management/resource_pool.h>

#include "recording/stream_recorder.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "camera/camera_pool.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include "camera/video_camera.h"
#include "core/misc/schedule_task.h"
#include "server_stream_recorder.h"
#include <utils/common/log.h>
#include "utils/common/synctime.h"
#include "api/app_server_connection.h"
#include "plugins/storage/dts/abstract_dts_reader_factory.h"
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include <business/business_event_connector.h>
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
#include "mutex/distributed_mutex_manager.h"

static const qint64 LICENSE_RECORDING_STOP_TIME = 60 * 24 * 30;
static const QString LICENSE_OVERFLOW_LOCK_NAME(lit("__LICENSE_OVERFLOW__"));

class QnServerDataProviderFactory: public QnDataProviderFactory
{
public:
    static QnServerDataProviderFactory* instance();
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(const QnResourcePtr& res, Qn::ConnectionRole role) override;
};

QnRecordingManager::LockData::LockData(const LockData& other)
{
    assert(0);
}

QnRecordingManager::LockData::LockData(LockData&& other): 
    mutex(other.mutex), 
    cameraResource(other.cameraResource), 
    currentTime(other.currentTime) 
{
    other.mutex = 0;
    other.cameraResource.clear();
    other.currentTime = 0;
}

QnRecordingManager::LockData::LockData():
    mutex(0), 
    currentTime(0) 
{
}

QnRecordingManager::LockData::LockData(ec2::QnDistributedMutex* mutex, QnVirtualCameraResourcePtr cameraResource, qint64 currentTime): 
    mutex(mutex), 
    cameraResource(cameraResource), 
    currentTime(currentTime) 
{
}

QnRecordingManager::LockData::~LockData() 
{
    if (mutex)
        mutex->deleteLater();
}

QnRecordingManager::QnRecordingManager(): m_mutex(QnMutex::Recursive)
{
    m_tooManyRecordingCnt = 0;
    m_licenseMutex = 0;
    connect(this, &QnRecordingManager::recordingDisabled, qnBusinessRuleConnector, &QnBusinessEventConnector::at_licenseIssueEvent);
    m_recordingStopTime = qMin(LICENSE_RECORDING_STOP_TIME, MSSettings::roSettings()->value("forceStopRecordingTime", LICENSE_RECORDING_STOP_TIME).toLongLong());
    m_recordingStopTime *= 1000 * 60;

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnRecordingManager::onNewResource, Qt::QueuedConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnRecordingManager::onRemoveResource, Qt::QueuedConnection);
    connect(&m_scheduleWatchingTimer, &QTimer::timeout, this, &QnRecordingManager::onTimer);
    connect(&m_licenseTimer, &QTimer::timeout, this, &QnRecordingManager::at_checkLicenses);
}

QnRecordingManager::~QnRecordingManager()
{
    stop();
}

void QnRecordingManager::start()
{
    m_scheduleWatchingTimer.start(1000);
    m_licenseTimer.start(1000 * 60);
    QThread::start();
}

void QnRecordingManager::beforeDeleteRecorder(const Recorders& recorders)
{
    if( recorders.recorderHiRes )
        recorders.recorderHiRes->pleaseStop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->pleaseStop();
}

void QnRecordingManager::stopRecorder(const Recorders& recorders)
{
    if( recorders.recorderHiRes )
        recorders.recorderHiRes->stop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->stop();
}

void QnRecordingManager::deleteRecorder(const Recorders& recorders, const QnResourcePtr& /*resource*/)
{
	QnVideoCamera* camera = 0;
    if (recorders.recorderHiRes) {
        recorders.recorderHiRes->stop();
		camera = qnCameraPool->getVideoCamera(recorders.recorderHiRes->getResource());
	}
    if (recorders.recorderLowRes) {
        recorders.recorderLowRes->stop();
        if (!camera)
            camera = qnCameraPool->getVideoCamera(recorders.recorderLowRes->getResource());
	}
    if (camera)
    {
        if (recorders.recorderHiRes) {
            const QnAbstractMediaStreamDataProviderPtr& reader = camera->getLiveReader(QnServer::HiQualityCatalog);
            if (reader)
                reader->removeDataProcessor(recorders.recorderHiRes);
        }

        if (recorders.recorderLowRes) {
            const QnAbstractMediaStreamDataProviderPtr& reader = camera->getLiveReader(QnServer::LowQualityCatalog);
            if (reader)
                reader->removeDataProcessor(recorders.recorderLowRes);
        }
    }
    delete recorders.recorderHiRes;
    delete recorders.recorderLowRes;
}

void QnRecordingManager::stop()
{
    exit();
    wait(); // stop QT event loop

    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    for(const Recorders& recorders: m_recordMap.values())
    {
        beforeDeleteRecorder(recorders);
    }
    for( QMap<QnResourcePtr, Recorders>::const_iterator
        it = m_recordMap.cbegin();
        it != m_recordMap.cend();
        ++it )
    {
        deleteRecorder(it.value(), it.key());
    }
    m_recordMap.clear();
    m_scheduleWatchingTimer.stop();

    onTimer();
    m_delayedStop.clear();
}

Recorders QnRecordingManager::findRecorders(const QnResourcePtr& res) const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_recordMap.value(res);
}

QnServerStreamRecorder* QnRecordingManager::createRecorder(const QnResourcePtr &res, QnVideoCamera* camera, QnServer::ChunksCatalog catalog)
{
    const QnAbstractMediaStreamDataProviderPtr& reader = camera->getLiveReader(catalog);
    if (reader == 0)
        return 0;
    QnServerStreamRecorder* recorder = new QnServerStreamRecorder(res, catalog, reader.data());
    recorder->setTruncateInterval(RECORDING_CHUNK_LEN);
    reader->addDataProcessor(recorder);
    reader->setNeedKeyData();
    //recorder->start();
    return recorder;
}

bool QnRecordingManager::isResourceDisabled(const QnResourcePtr& res) const
{
    if (res->hasFlags(Qn::foreigner))
        return true;

    const QnVirtualCameraResource* cameraRes = dynamic_cast<const QnVirtualCameraResource*>(res.data());
    return  cameraRes && cameraRes->isScheduleDisabled();
}

void QnRecordingManager::updateCameraHistory(const QnResourcePtr& res)
{
    const QnVirtualCameraResourcePtr netRes = res.dynamicCast<QnVirtualCameraResource>();

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    QnMediaServerResourcePtr currentServer = QnCameraHistoryPool::instance()->getMediaServerOnTime(netRes, currentTime, true);
    if (currentServer && currentServer->getId() == qnCommon->moduleGUID())
        return; // camera history already inserted. skip

    QString name = ec2::QnMutexCameraDataHandler::CAM_HISTORY_PREFIX;
    name.append(netRes->getPhysicalId());
    if (m_lockInProgress.find(name) != m_lockInProgress.end())
        return; // operation in progress

    ec2::QnDistributedMutex* mutex = ec2::QnDistributedMutexManager::instance()->createMutex(name);
    connect(mutex, &ec2::QnDistributedMutex::locked, this, &QnRecordingManager::at_historyMutexLocked, Qt::QueuedConnection);
    connect(mutex, &ec2::QnDistributedMutex::lockTimeout, this, &QnRecordingManager::at_historyMutexTimeout, Qt::QueuedConnection);
    
    m_lockInProgress.emplace(name, LockData(mutex, netRes, currentTime));

    mutex->lockAsync();
}

void QnRecordingManager::at_historyMutexTimeout()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    if (mutex)
        m_lockInProgress.erase(mutex->name());
}

void QnRecordingManager::at_historyMutexLocked()
{
	SCOPED_MUTEX_LOCK( lock, &m_mutex);
    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    if (!mutex)
        return;
    auto itr = m_lockInProgress.find(mutex->name());
    if (itr == m_lockInProgress.end())
        return;
    const LockData& data = itr->second;

    if (mutex->checkUserData())
    {
        QnCameraHistoryItem cameraHistoryItem(data.cameraResource->getUniqueId(), data.currentTime, qnCommon->moduleGUID());

        const ec2::AbstractECConnectionPtr& appServerConnection = QnAppServerConnectionFactory::getConnection2();
        ec2::ErrorCode errCode = appServerConnection->getCameraManager()->addCameraHistoryItemSync(cameraHistoryItem);
        if (errCode == ec2::ErrorCode::ok)
            QnCameraHistoryPool::instance()->addCameraHistoryItem(cameraHistoryItem);
        else
            qCritical() << "ECS server error during execute method addCameraHistoryItem: " << ec2::toString(errCode);
    }

    mutex->unlock();
    m_lockInProgress.erase(mutex->name());
}

bool QnRecordingManager::startForcedRecording(const QnSecurityCamResourcePtr& camRes, Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDuration)
{
    updateCamera(camRes); // ensure recorders are created
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(camRes);
    if (!camera)
        return false;

    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QMap<QnResourcePtr, Recorders>::const_iterator itrRec = m_recordMap.find(camRes);
    if (itrRec == m_recordMap.constEnd())
        return false;

    // update current schedule task
    const Recorders& recorders = itrRec.value();
    if (recorders.recorderHiRes)
        recorders.recorderHiRes->startForcedRecording(quality, fps, beforeThreshold, afterThreshold, maxDuration);
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->startForcedRecording(quality, fps, beforeThreshold, afterThreshold, maxDuration);
    
    // start recorder threads
    startOrStopRecording(camRes, camera, recorders.recorderHiRes, recorders.recorderLowRes);

    // return true if recording started. if camera is not accessible e.t.c return false
    return true;
}

bool QnRecordingManager::stopForcedRecording(const QnSecurityCamResourcePtr& camRes, bool afterThresholdCheck)
{
    updateCamera(camRes); // ensure recorders are created
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(camRes);
    if (!camera)
        return false;

    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QMap<QnResourcePtr, Recorders>::iterator itrRec = m_recordMap.find(camRes);
    if (itrRec == m_recordMap.end())
        return false;

    const Recorders& recorders = itrRec.value();
    if (!recorders.recorderHiRes && !recorders.recorderLowRes)
        return false;

    int afterThreshold = recorders.recorderHiRes ? recorders.recorderHiRes->getFRAfterThreshold() : recorders.recorderLowRes->getFRAfterThreshold();
    if (afterThreshold > 0 && afterThresholdCheck) {
        m_delayedStop.insert(camRes, qnSyncTime->currentUSecsSinceEpoch() + afterThreshold*1000000ll);
        return true;
    }
    m_delayedStop.remove(camRes);

    // update current schedule task
    if (recorders.recorderHiRes)
        recorders.recorderHiRes->stopForcedRecording();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->stopForcedRecording();

    // start recorder threads
    startOrStopRecording(camRes, camera, recorders.recorderHiRes, recorders.recorderLowRes);
    return true;
}

bool QnRecordingManager::startOrStopRecording(const QnResourcePtr& res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes)
{
    QnSecurityCamResourcePtr cameraRes = res.dynamicCast<QnSecurityCamResource>();
    bool needRecordCamera = !isResourceDisabled(res) && !cameraRes->isDtsBased();
    if (!cameraRes->isInitialized() && needRecordCamera) {
        cameraRes->initAsync(true);
        return false; // wait for initialization
    }

    bool someRecordingIsPresent = false;

    //QnStorageManager* storageMan = QnStorageManager::instance();
    if (needRecordCamera && res->getStatus() != Qn::Offline /* && storageMan->rebuildState() == QnStorageManager::RebuildState_None*/)
    {
        QnLiveStreamProviderPtr providerHi = camera->getLiveReader(QnServer::HiQualityCatalog);
        QnLiveStreamProviderPtr providerLow = camera->getLiveReader(QnServer::LowQualityCatalog);

        someRecordingIsPresent = true;

        if (providerHi)
        {
            if (recorderHiRes) {
                if (!recorderHiRes->isRunning()) {
                    NX_LOG(QString(lit("Recording started for camera %1")).arg(res->getUniqueId()), cl_logINFO);
                    recorderHiRes->start();
                    camera->inUse(recorderHiRes);
                }
            }
            providerHi->startIfNotRunning();
        }

        if (recorderLowRes)
        {
            float currentFps = recorderHiRes ? recorderHiRes->currentScheduleTask().getFps() : 0;

            // second stream should run if camera do not share fps or at least MIN_SECONDARY_FPS frames left for second stream
            bool runSecondStream = (cameraRes->streamFpsSharingMethod() != Qn::BasicFpsSharing || cameraRes->getMaxFps() - currentFps >= MIN_SECONDARY_FPS) && 
                                    cameraRes->hasDualStreaming2() && providerLow;
            if (runSecondStream)
            {
                if (recorderLowRes) {
                    if (!recorderLowRes->isRunning()) {
                        NX_LOG(QString(lit("Recording started (secondary stream) for camera  %1")).arg(res->getUniqueId()), cl_logINFO);
                        recorderLowRes->start();
                        camera->inUse(recorderLowRes);
                    }
                }
                providerLow->startIfNotRunning();
            }
            else {
                if (recorderLowRes && recorderLowRes->isRunning()) {
                    recorderLowRes->pleaseStop();
                    camera->notInUse(recorderLowRes);
                }
                camera->updateActivity();
            }
        }
        if (recorderHiRes || recorderLowRes)
            updateCameraHistory(res);
    }
    else 
    {
        bool needStopHi = recorderHiRes && recorderHiRes->isRunning();
        bool needStopLow = recorderLowRes && recorderLowRes->isRunning();

        if (needStopHi || needStopLow)
            someRecordingIsPresent = true;

        if (needStopHi) {
            recorderHiRes->pleaseStop();
            camera->notInUse(recorderHiRes);
        }
        if (needStopLow) {
            recorderLowRes->pleaseStop();
            camera->notInUse(recorderLowRes);
        }

        camera->updateActivity();

        if (needStopHi) {
            NX_LOG(QString(lit("Recording stopped for camera %1")).arg(res->getUniqueId()), cl_logINFO);
        }
        if (!res->hasFlags(Qn::foreigner)) {
            if(!needStopHi && !needStopLow && res->getStatus() == Qn::Recording)
                res->setStatus(Qn::Online); // may be recording thread was not runned, so reset status to online
        }
    }

    return someRecordingIsPresent;
}

void QnRecordingManager::updateCamera(const QnSecurityCamResourcePtr& cameraRes)
{
    const QnResourcePtr& res = cameraRes;
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(cameraRes);
    QnAbstractMediaStreamDataProviderPtr providerHi = camera->getLiveReader(QnServer::HiQualityCatalog);
    QnAbstractMediaStreamDataProviderPtr providerLow = camera->getLiveReader(QnServer::LowQualityCatalog);

    //if (cameraRes->getMotionType() == MT_SoftwareGrid)


    if (camera)
    {
        QMap<QnResourcePtr, Recorders>::iterator itrRec = m_recordMap.find(res);
        if (itrRec != m_recordMap.end())
        {
            Recorders& recorders = itrRec.value();
            if (recorders.recorderHiRes)
                recorders.recorderHiRes->updateCamera(cameraRes);

            if (recorders.recorderHiRes && providerLow && !recorders.recorderLowRes)
            {
                recorders.recorderLowRes = createRecorder(res, camera, QnServer::LowQualityCatalog);
                if (recorders.recorderLowRes)
                    recorders.recorderLowRes->setDualStreamingHelper(recorders.recorderHiRes->getDualStreamingHelper());
            }
            if (recorders.recorderLowRes)
                recorders.recorderLowRes->updateCamera(cameraRes);

            startOrStopRecording(res, camera, recorders.recorderHiRes, recorders.recorderLowRes);
        }
        else if (!cameraRes->hasFlags(Qn::foreigner))
        {
            QnServerStreamRecorder* recorderHiRes = createRecorder(res, camera, QnServer::HiQualityCatalog);
            QnServerStreamRecorder* recorderLowRes = createRecorder(res, camera, QnServer::LowQualityCatalog);

            if (!recorderHiRes && !recorderLowRes)
                return;
            
            QnDualStreamingHelperPtr dialStreamingHelper(new QnDualStreamingHelper());
            if (recorderHiRes)
                recorderHiRes->setDualStreamingHelper(dialStreamingHelper);
            if (recorderLowRes)
                recorderLowRes->setDualStreamingHelper(dialStreamingHelper);

            m_mutex.lock();
            m_recordMap.insert(res, Recorders(recorderHiRes, recorderLowRes));
            m_mutex.unlock();

            if (recorderHiRes)
                recorderHiRes->updateCamera(cameraRes);
            if (recorderLowRes)
                recorderLowRes->updateCamera(cameraRes);

            startOrStopRecording(res, camera, recorderHiRes, recorderLowRes);
        }
    }
}

void QnRecordingManager::at_camera_resourceChanged(const QnResourcePtr &resource)
{
    Q_UNUSED(resource)

    QnVirtualCameraResource* cameraPtr = dynamic_cast<QnVirtualCameraResource*>(sender());
    if( !cameraPtr )
        return;

    const QnVirtualCameraResourcePtr& camera = cameraPtr->toSharedPointer().staticCast<QnVirtualCameraResource>();
    bool ownResource = !camera->hasFlags(Qn::foreigner);
    if (ownResource && !camera->isInitialized())
        camera->initAsync(false);

    updateCamera(camera);

    // no need mutex here because of readOnly access from other threads and m_recordMap modified only from current thread
    //SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QMap<QnResourcePtr, Recorders>::const_iterator itr = m_recordMap.find(camera); // && m_recordMap.value(camera).recorderHiRes->isRunning();
    if (itr != m_recordMap.constEnd() && ownResource) 
    {
        if (itr->recorderHiRes && itr->recorderHiRes->isAudioPresent() != camera->isAudioEnabled()) 
            itr->recorderHiRes->setNeedReopen();
        if (itr->recorderLowRes && itr->recorderLowRes->isAudioPresent() != camera->isAudioEnabled()) 
            itr->recorderLowRes->setNeedReopen();
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
    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(resource);
    if (camera) 
    {
        connect(camera.data(), &QnResource::initializedChanged, this, &QnRecordingManager::at_camera_initializationChanged);
        connect(camera.data(), &QnResource::resourceChanged,    this, &QnRecordingManager::at_camera_resourceChanged);
        camera->setDataProviderFactory(QnServerDataProviderFactory::instance());
        if (camera->isInitialized())
            updateCamera(camera);
    }
}

void QnRecordingManager::onRemoveResource(const QnResourcePtr &resource)
{
    QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (physicalStorage) {
        qnStorageMan->removeStorage(physicalStorage);
        return;
    }

    Recorders recorders;
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        QMap<QnResourcePtr, Recorders>::const_iterator itr = m_recordMap.find(resource);
        if (itr == m_recordMap.constEnd())
            return;
        recorders = itr.value();
        m_recordMap.remove(resource);
    }

    beforeDeleteRecorder(recorders);
    stopRecorder(recorders);
    qnCameraPool->removeVideoCamera(resource);
    deleteRecorder(recorders, resource);
}

bool QnRecordingManager::isCameraRecoring(const QnResourcePtr& camera)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    QMap<QnResourcePtr, Recorders>::const_iterator itr = m_recordMap.find(camera);
    if (itr == m_recordMap.end())
        return false;
    return (itr.value().recorderHiRes && itr.value().recorderHiRes->isRunning()) ||
           (itr.value().recorderLowRes && itr.value().recorderLowRes->isRunning());
}

void QnRecordingManager::onTimer()
{
    qint64 time = qnSyncTime->currentMSecsSinceEpoch();

    // Mutex is not required here because of m_recordMap used in readOnly mode here and m_recordMap modified from this thread only (from other private slot)
    //SCOPED_MUTEX_LOCK( lock, &m_mutex);

    bool someRecordingIsPresent = false;
    for (QMap<QnResourcePtr, Recorders>::const_iterator itrRec = m_recordMap.constBegin(); itrRec != m_recordMap.constEnd(); ++itrRec)
    {
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(itrRec.key());
        const Recorders& recorders = itrRec.value();

        if (!recorders.recorderHiRes && !recorders.recorderLowRes)
            return; // no recorders are created now

        if (recorders.recorderHiRes)
            recorders.recorderHiRes->updateScheduleInfo(time);
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->updateScheduleInfo(time);
        someRecordingIsPresent |= startOrStopRecording(itrRec.key(), camera, recorders.recorderHiRes, recorders.recorderLowRes);
    }

    QnStorageManager* storageMan = QnStorageManager::instance();
    if (!someRecordingIsPresent && storageMan->rebuildState() == QnStorageManager::RebuildState_WaitForRecordersStopped)
    {
        storageMan->setRebuildState(QnStorageManager::RebuildState_Started);
    }

    QMap<QnSecurityCamResourcePtr, qint64> stopList = m_delayedStop;
    for (QMap<QnSecurityCamResourcePtr, qint64>::iterator itrDelayedStop = stopList.begin(); itrDelayedStop != stopList.end(); ++itrDelayedStop)
    {
        qint64 stopTime = itrDelayedStop.value();
        if (stopTime <= time*1000ll)
            stopForcedRecording(itrDelayedStop.key(), false);
    }
}

QnVirtualCameraResourceList QnRecordingManager::getLocalControlledCameras() const
{
    // return own cameras + cameras from servers without DB (remote connected servers)
    QnVirtualCameraResourceList cameras = qnResPool->getAllCameras(QnResourcePtr());
    QnVirtualCameraResourceList result;
    for(const QnVirtualCameraResourcePtr &camRes: cameras)
    {
        QnMediaServerResourcePtr mServer = camRes->getParentResource().dynamicCast<QnMediaServerResource>();
        if (!mServer)
            continue;
        if (mServer->getId() == qnCommon->moduleGUID() || (mServer->getServerFlags() | Qn::SF_RemoteEC))
            result << camRes;
    }
    return result;
}

void QnRecordingManager::at_checkLicenses()
{
    if (m_licenseMutex)
        return;

    QnCamLicenseUsageHelper helper;

    if (!helper.isValid())
    {
        if (++m_tooManyRecordingCnt < 5)
            return; // do not report license problem immediately. Server should wait several minutes, probably other servers will be available soon


        qint64 licenseOverflowTime = QnRuntimeInfoManager::instance()->localInfo().data.prematureLicenseExperationDate;
        if (licenseOverflowTime == 0) {
            licenseOverflowTime = qnSyncTime->currentMSecsSinceEpoch();
            QnAppServerConnectionFactory::getConnection2()->getMiscManager()->markLicenseOverflowSync(true, licenseOverflowTime);
        }
        if (qnSyncTime->currentMSecsSinceEpoch() - licenseOverflowTime < m_recordingStopTime) {
            return; // not enough license, but timeout not reached yet
        }

        // Too many licenses. check if server has own recording cameras and force to disable recording
        QnVirtualCameraResourceList ownCameras = getLocalControlledCameras();
        for(const QnVirtualCameraResourcePtr& camera: ownCameras)
        {
            if (helper.isOverflowForCamera(camera))
            {
                // found. remove recording from some of them

                m_licenseMutex = ec2::QnDistributedMutexManager::instance()->createMutex(LICENSE_OVERFLOW_LOCK_NAME);
                connect(m_licenseMutex, &ec2::QnDistributedMutex::locked, this, &QnRecordingManager::at_licenseMutexLocked, Qt::QueuedConnection);
                connect(m_licenseMutex, &ec2::QnDistributedMutex::lockTimeout, this, &QnRecordingManager::at_licenseMutexTimeout, Qt::QueuedConnection);
                m_licenseMutex->lockAsync();
                helper.invalidate();
                break;
            }
        }
    }
    else {
        qint64 licenseOverflowTime = QnRuntimeInfoManager::instance()->localInfo().data.prematureLicenseExperationDate;
        if (licenseOverflowTime)
            QnAppServerConnectionFactory::getConnection2()->getMiscManager()->markLicenseOverflowSync(false, 0);
        m_tooManyRecordingCnt = 0;
    }
}

void QnRecordingManager::at_licenseMutexLocked()
{
    QnCamLicenseUsageHelper helper;

    QString disabledCameras;
    
    // Too many licenses. check if server has own recording cameras and force to disable recording
    const QnVirtualCameraResourceList& ownCameras = getLocalControlledCameras();
    for(const QnVirtualCameraResourcePtr& camera: ownCameras)
    {
        if (helper.isValid())
            break;

        if (helper.isOverflowForCamera(camera))
        {
            camera->setScheduleDisabled(true);
            QList<QnUuid> idList;
            idList << camera->getId();
            ec2::ErrorCode errCode =  QnAppServerConnectionFactory::getConnection2()->getCameraManager()->saveUserAttributesSync(QnCameraUserAttributePool::instance()->getAttributesList(idList));
            if (errCode != ec2::ErrorCode::ok)
            {
                qWarning() << "Can't turn off recording for camera:" << camera->getUniqueId() << "error:" << ec2::toString(errCode);
                camera->setScheduleDisabled(false); // rollback
                continue;
            }
            propertyDictionary->saveParams( camera->getId() );
            disabledCameras += QString(lit("%1 (%2)")).arg(camera->getName()).arg(camera->getHostAddress());
            helper.invalidate();
        }
    }
    m_licenseMutex->unlock();
    m_licenseMutex->deleteLater();
    m_licenseMutex = 0;

    if (!disabledCameras.isEmpty()) {
        QnResourcePtr resource = qnResPool->getResourceById(qnCommon->moduleGUID());
        emit recordingDisabled(resource, qnSyncTime->currentUSecsSinceEpoch(), QnBusiness::LicenseRemoved, disabledCameras);
    }
}

void QnRecordingManager::at_licenseMutexTimeout()
{
    m_licenseMutex->deleteLater();
    m_licenseMutex = 0;
}

//Q_GLOBAL_STATIC(QnRecordingManager, qn_recordingManager_instance)
static QnRecordingManager* staticInstance = NULL;

void QnRecordingManager::initStaticInstance( QnRecordingManager* recordingManager )
{
    staticInstance = recordingManager;
}

QnRecordingManager* QnRecordingManager::instance()
{
    //return qn_recordingManager_instance();
    return staticInstance;
}

// --------------------- QnServerDataProviderFactory -------------------
Q_GLOBAL_STATIC(QnServerDataProviderFactory, qn_serverDataProviderFactory_instance)

QnAbstractStreamDataProvider* QnServerDataProviderFactory::createDataProviderInternal(const QnResourcePtr& res, Qn::ConnectionRole role)
{
    QnVirtualCameraResourcePtr vcRes = qSharedPointerDynamicCast<QnVirtualCameraResource>(res);


    bool sholdBeDts = (role == Qn::CR_Archive) && vcRes && vcRes->getDTSFactory();
    if (sholdBeDts)
    {
        QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(res);
        archiveReader->setCycleMode(false);

        vcRes->lockDTSFactory();

        QnAbstractArchiveDelegate* del = vcRes->getDTSFactory()->createDeligate(vcRes);

        vcRes->unLockDTSFactory();

        archiveReader->setArchiveDelegate(del);
        return archiveReader;
        
    }
    else  if (role == Qn::CR_Archive)
    {
        QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(res);
        archiveReader->setCycleMode(false);
        archiveReader->setArchiveDelegate(new QnServerArchiveDelegate());
        return archiveReader;
    }
    return 0;
}

QnServerDataProviderFactory* QnServerDataProviderFactory::instance()
{
    return qn_serverDataProviderFactory_instance();
}


