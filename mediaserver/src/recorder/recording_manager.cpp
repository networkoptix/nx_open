#include "recording_manager.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/security_cam_resource.h"
#include "recording/stream_recorder.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "camera/camera_pool.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "camera/video_camera.h"
#include "core/misc/schedule_task.h"
#include "server_stream_recorder.h"
#include "utils/common/synctime.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/camera_resource.h"
#include "core/resource/camera_history.h"
#include "api/app_server_connection.h"
#include "plugins/storage/dts/abstract_dts_reader_factory.h"
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include <business/business_event_connector.h>
#include <media_server/serverutil.h>

QnRecordingManager::QnRecordingManager(): m_mutex(QMutex::Recursive)
{
}

QnRecordingManager::~QnRecordingManager()
{
    stop();
}

void QnRecordingManager::start()
{
    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(onNewResource(QnResourcePtr)), Qt::QueuedConnection);
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(onRemoveResource(QnResourcePtr)), Qt::QueuedConnection);
    connect(&m_scheduleWatchingTimer, SIGNAL(timeout()), this, SLOT(onTimer()));

    m_scheduleWatchingTimer.start(1000);

    QThread::start();
}

void QnRecordingManager::beforeDeleteRecorder(const Recorders& recorders)
{
    if( recorders.recorderHiRes )
        recorders.recorderHiRes->pleaseStop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->pleaseStop();
}

void QnRecordingManager::deleteRecorder(const Recorders& recorders)
{
    if (recorders.recorderHiRes)
        recorders.recorderHiRes->stop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->stop();
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(recorders.recorderHiRes->getResource());
    if (camera)
    {
        if (recorders.recorderHiRes) {
            QnAbstractMediaStreamDataProviderPtr reader = camera->getLiveReader(QnResource::Role_LiveVideo);
            if (reader)
                reader->removeDataProcessor(recorders.recorderHiRes);
        }

        if (recorders.recorderLowRes) {
            QnAbstractMediaStreamDataProviderPtr reader = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);
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

    QMutexLocker lock(&m_mutex);

    foreach(const Recorders& recorders, m_recordMap.values())
    {
        beforeDeleteRecorder(recorders);
    }
    foreach(const Recorders& recorders, m_recordMap.values()) 
    {
        deleteRecorder(recorders);
    }
    m_recordMap.clear();
    m_onlineCameras.clear();
    m_scheduleWatchingTimer.stop();

    onTimer();
    m_delayedStop.clear();
}

Recorders QnRecordingManager::findRecorders(QnResourcePtr res) const
{
    QMutexLocker lock(&m_mutex);
    return m_recordMap.value(res);
}

QnServerStreamRecorder* QnRecordingManager::createRecorder(QnResourcePtr res, QnVideoCamera* camera, QnResource::ConnectionRole role)
{
    QnAbstractMediaStreamDataProviderPtr reader = camera->getLiveReader(role);
    if (reader == 0)
        return 0;
    QnServerStreamRecorder* recorder = new QnServerStreamRecorder(res, role, reader.data());
    recorder->setTruncateInterval(RECORDING_CHUNK_LEN);
    reader->addDataProcessor(recorder);
    reader->setNeedKeyData();
    //recorder->start();
    return recorder;
}

bool QnRecordingManager::isResourceDisabled(QnResourcePtr res) const
{
    if (res->isDisabled())
        return true;

    QnVirtualCameraResourcePtr cameraRes = qSharedPointerDynamicCast<QnVirtualCameraResource>(res);
    return  cameraRes && cameraRes->isScheduleDisabled();
}

bool QnRecordingManager::updateCameraHistory(QnResourcePtr res)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
    QString physicalId = netRes->getPhysicalId();
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    if (QnCameraHistoryPool::instance()->getMinTime(netRes) == (qint64)AV_NOPTS_VALUE)
    {
        // it is first record for camera
        DeviceFileCatalogPtr catalogHi = qnStorageMan->getFileCatalog(physicalId, QnResource::Role_LiveVideo);
        qint64 archiveMinTime = catalogHi->minTime();
        if (archiveMinTime != (qint64)AV_NOPTS_VALUE)
            currentTime = qMin(currentTime,  archiveMinTime);
    }

    QnMediaServerResourcePtr server = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(res->getParentId()));
    QnCameraHistoryItem cameraHistoryItem(netRes->getPhysicalId(), currentTime, server->getGuid());
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
    if (appServerConnection->addCameraHistoryItem(cameraHistoryItem) != 0) {
        qCritical() << "ECS server error during execute method addCameraHistoryItem: " << appServerConnection->getLastError();
        return false;
    }
    return true;
}

bool QnRecordingManager::startForcedRecording(QnSecurityCamResourcePtr camRes, Qn::StreamQuality quality, int fps, int beforeThreshold, int afterThreshold, int maxDuration)
{
    updateCamera(camRes); // ensure recorders are created
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(camRes);
    if (!camera)
        return false;

    QMutexLocker lock(&m_mutex);

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

bool QnRecordingManager::stopForcedRecording(QnSecurityCamResourcePtr camRes, bool afterThresholdCheck)
{
    updateCamera(camRes); // ensure recorders are created
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(camRes);
    if (!camera)
        return false;

    QMutexLocker lock(&m_mutex);

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

bool QnRecordingManager::startOrStopRecording(QnResourcePtr res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes)
{
    QnLiveStreamProviderPtr providerHi = camera->getLiveReader(QnResource::Role_LiveVideo);
    QnLiveStreamProviderPtr providerLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);
    QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);

    if (!cameraRes->isInitialized() && !cameraRes->isDisabled() && !cameraRes->isScheduleDisabled())
        cameraRes->initAsync(true);

    bool someRecordingIsPresent = false;

    //QnStorageManager* storageMan = QnStorageManager::instance();
    if (!isResourceDisabled(res) && !cameraRes->isDtsBased() && res->getStatus() != QnResource::Offline /* && storageMan->rebuildState() == QnStorageManager::RebuildState_None*/)
    {
        someRecordingIsPresent = true;

        if (providerHi)
        {
            if (recorderHiRes) {
                if (!recorderHiRes->isRunning()) {
                    if (!updateCameraHistory(res))
                        return someRecordingIsPresent;
                    NX_LOG(QString(lit("Recording started for camera %1")).arg(res->getUniqueId()), cl_logINFO);
                }
                recorderHiRes->start();
            }
            providerHi->startIfNotRunning();
        }

        if (providerLow && recorderHiRes) {
            float currentFps = recorderHiRes->currentScheduleTask().getFps();

            // second stream should run if camera do not share fps or at least MIN_SECONDARY_FPS frames left for second stream
            bool runSecondStream = (cameraRes->streamFpsSharingMethod() != Qn::shareFps || cameraRes->getMaxFps() - currentFps >= MIN_SECONDARY_FPS) && cameraRes->hasDualStreaming2(); 

            if (runSecondStream)
            {
                if (recorderLowRes) {
                    if (!recorderLowRes->isRunning()) {
                        NX_LOG(QString(lit("Recording started (secondary stream) for camera  %1")).arg(res->getUniqueId()), cl_logINFO);
                    }
                    recorderLowRes->start();
                }
                providerLow->startIfNotRunning();
            }
            else {
                if (recorderLowRes)
                    recorderLowRes->pleaseStop();
                providerLow->pleaseStop();
                //if (recorderLowRes)
                //    recorderLowRes->clearUnprocessedData();
            }
        }
    }
    else 
    {
        bool needStopHi = recorderHiRes && recorderHiRes->isRunning();
        bool needStopLow = recorderLowRes && recorderLowRes->isRunning();

        if (needStopHi || needStopLow)
            someRecordingIsPresent = true;

        if (needStopHi)
            recorderHiRes->pleaseStop();
        if (needStopLow)
            recorderLowRes->pleaseStop();

        /*
        if (needStopHi)
            recorderHiRes->stop();
        if (needStopLow)
            recorderLowRes->stop();
        */
        camera->updateActivity();
        /*
        if (needStopHi)
            recorderHiRes->clearUnprocessedData();
        if (needStopLow)
            recorderLowRes->clearUnprocessedData();
        */

        if (needStopHi) {
            NX_LOG(QString(lit("Recording stopped for camera %1")).arg(res->getUniqueId()), cl_logINFO);
        }
        if(!needStopHi && !needStopLow && res->getStatus() == QnResource::Recording)
            res->setStatus(QnResource::Online); // may be recording thread was not runned, so reset status to online
    }

    return someRecordingIsPresent;
}

void QnRecordingManager::updateCamera(QnSecurityCamResourcePtr res)
{
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);
    QnAbstractMediaStreamDataProviderPtr providerHi = camera->getLiveReader(QnResource::Role_LiveVideo);
    QnAbstractMediaStreamDataProviderPtr providerLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);

    //if (cameraRes->getMotionType() == MT_SoftwareGrid)


    if (camera)
    {
        QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
        cameraRes->setDataProviderFactory(QnServerDataProviderFactory::instance());


        QMap<QnResourcePtr, Recorders>::const_iterator itrRec = m_recordMap.find(res);
        if (itrRec != m_recordMap.constEnd())
        {
            const Recorders& recorders = itrRec.value();
            if (recorders.recorderHiRes)
                recorders.recorderHiRes->updateCamera(res);

            if (recorders.recorderHiRes && providerLow && !recorders.recorderLowRes)
            {
                QnServerStreamRecorder* recorderLowRes = createRecorder(res, camera, QnResource::Role_SecondaryLiveVideo);
                if (recorderLowRes) 
                    recorderLowRes->setDualStreamingHelper(recorders.recorderHiRes->getDualStreamingHelper());
            }
            if (recorders.recorderLowRes)
                recorders.recorderLowRes->updateCamera(res);

            startOrStopRecording(res, camera, recorders.recorderHiRes, recorders.recorderLowRes);
        }
        else if (!res->isDisabled())
        {
            QnServerStreamRecorder* recorderHiRes = createRecorder(res, camera, QnResource::Role_LiveVideo);
            QnServerStreamRecorder* recorderLowRes = createRecorder(res, camera, QnResource::Role_SecondaryLiveVideo);

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

void QnRecordingManager::at_camera_initAsyncFinished(const QnResourcePtr &resource, bool state)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera && state)
        updateCamera(camera);
}

void QnRecordingManager::at_camera_resourceChanged(const QnResourcePtr &resource)
{
    Q_UNUSED(resource)

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource> (dynamic_cast<QnVirtualCameraResource*>(sender())->toSharedPointer());
    if (camera) {
        if (!camera->isInitialized() && !camera->isDisabled()) {
            camera->initAsync(false);
        }

        QnResourcePtr mServer = qnResPool->getResourceById(camera->getParentId());
        if (!mServer || mServer->getGuid() != serverGuid())
            return; // it is camera from other server

        updateCamera(camera);

        // no need mutex here because of readOnly access from other threads and m_recordMap modified only from current thread
        //QMutexLocker lock(&m_mutex);

        QMap<QnResourcePtr, Recorders>::const_iterator itr = m_recordMap.find(camera); // && m_recordMap.value(camera).recorderHiRes->isRunning();
        if (itr != m_recordMap.constEnd()) 
        {
            if (itr->recorderHiRes && itr->recorderHiRes->isAudioPresent() != camera->isAudioEnabled()) 
                itr->recorderHiRes->setNeedReopen();
            if (itr->recorderLowRes && itr->recorderLowRes->isAudioPresent() != camera->isAudioEnabled()) 
                itr->recorderLowRes->setNeedReopen();
        }
    }
}

void QnRecordingManager::at_camera_statusChanged(const QnResourcePtr &resource)
{
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    QnResource::Status status = camera->getStatus();
    if((status == QnResource::Online || status == QnResource::Recording) && !m_onlineCameras.contains(camera)) {
        updateCamera(camera);
        m_onlineCameras.insert(camera);
    }

    if((status == QnResource::Offline || status == QnResource::Unauthorized) && m_onlineCameras.contains(camera)) 
        m_onlineCameras.remove(camera);
}

void QnRecordingManager::onNewResource(const QnResourcePtr &resource)
{
    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(resource);
    if (camera && !camera->hasFlags(QnResource::foreigner)) 
    {
        QnResource::Status status = camera->getStatus();
        if(status == QnResource::Online || status == QnResource::Recording)
            m_onlineCameras.insert(camera); // TODO: merge into at_camera_statusChanged

        if (!camera->isInitialized() && !camera->isDisabled() && !camera->isScheduleDisabled())
            camera->initAsync(true);

        connect(camera.data(), SIGNAL(statusChanged(const QnResourcePtr &)),            this, SLOT(at_camera_statusChanged(const QnResourcePtr &)));
        connect(camera.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),          this, SLOT(at_camera_resourceChanged(const QnResourcePtr &)));
        connect(camera.data(), SIGNAL(initAsyncFinished(const QnResourcePtr &, bool)),  this, SLOT(at_camera_initAsyncFinished(const QnResourcePtr &, bool)));
        updateCamera(camera);
        return;
    }

    QnMediaServerResourcePtr server = qSharedPointerDynamicCast<QnMediaServerResource>(resource);
    if (server && server->getGuid() == serverGuid())
        connect(server.data(), SIGNAL(resourceChanged(const QnResourcePtr &)), this, SLOT(at_server_resourceChanged(const QnResourcePtr &)), Qt::QueuedConnection);
}

void QnRecordingManager::at_server_resourceChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    qnStorageMan->removeAbsentStorages(server->getStorages());
    foreach(QnAbstractStorageResourcePtr storage, server->getStorages())
    {
        QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(storage);
        if (physicalStorage)
            qnStorageMan->addStorage(physicalStorage);
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
        QMutexLocker lock(&m_mutex);
        QMap<QnResourcePtr, Recorders>::const_iterator itr = m_recordMap.find(resource);
        if (itr == m_recordMap.constEnd())
            return;
        recorders = itr.value();
        m_recordMap.remove(resource);
    }
    qnCameraPool->removeVideoCamera(resource);

    beforeDeleteRecorder(recorders);
    deleteRecorder(recorders);

    m_onlineCameras.remove(resource);
}

bool QnRecordingManager::isCameraRecoring(QnResourcePtr camera)
{
    QMutexLocker lock(&m_mutex);
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
    //QMutexLocker lock(&m_mutex);

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

QnAbstractStreamDataProvider* QnServerDataProviderFactory::createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role)
{
    QnVirtualCameraResourcePtr vcRes = qSharedPointerDynamicCast<QnVirtualCameraResource>(res);


    bool sholdBeDts = (role == QnResource::Role_Archive) && vcRes && vcRes->getDTSFactory();
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
    else  if (role == QnResource::Role_Archive)
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


