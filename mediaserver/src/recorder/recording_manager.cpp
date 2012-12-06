#include "recording_manager.h"
#include "core/resource_managment/resource_pool.h"
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

QnRecordingManager::QnRecordingManager()
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
    recorders.recorderHiRes->pleaseStop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->pleaseStop();
}

void QnRecordingManager::deleteRecorder(const Recorders& recorders)
{
    recorders.recorderHiRes->stop();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->stop();
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(recorders.recorderHiRes->getResource());
    if (camera)
    {
        QnAbstractMediaStreamDataProviderPtr reader = camera->getLiveReader(QnResource::Role_LiveVideo);
        if (reader)
            reader->removeDataProcessor(recorders.recorderHiRes);

        if (recorders.recorderLowRes) {
            reader = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);
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
    if (QnCameraHistoryPool::instance()->getMinTime(netRes) == AV_NOPTS_VALUE)
    {
        // it is first record for camera
        DeviceFileCatalogPtr catalogHi = qnStorageMan->getFileCatalog(physicalId, QnResource::Role_LiveVideo);
        qint64 archiveMinTime = catalogHi->minTime();
        if (archiveMinTime != AV_NOPTS_VALUE)
            currentTime = qMin(currentTime,  archiveMinTime);
    }

    QnMediaServerResourcePtr server = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(res->getParentId()));
    QnCameraHistoryItem cameraHistoryItem(netRes->getPhysicalId(), currentTime, server->getGuid());
    QByteArray errStr;
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
    if (appServerConnection->addCameraHistoryItem(cameraHistoryItem, errStr) != 0) {
        qCritical() << "ECS server error during execute method addCameraHistoryItem: " << errStr;
        return false;
    }
    return true;
}

void QnRecordingManager::startOrStopRecording(QnResourcePtr res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes)
{
    QnAbstractMediaStreamDataProviderPtr providerHi = camera->getLiveReader(QnResource::Role_LiveVideo);
    QnAbstractMediaStreamDataProviderPtr providerLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);

    if (!isResourceDisabled(res) && res->getStatus() != QnResource::Offline && 
        recorderHiRes->currentScheduleTask().getRecordingType() != Qn::RecordingType_Never)
    {
        if (providerHi)
        {
            if (!recorderHiRes->isRunning()) {
                if (!updateCameraHistory(res))
                    return;
                cl_log.log("Recording started for camera ", res->getUniqueId(), cl_logINFO);
            }
            recorderHiRes->start();
            providerHi->start();
        }

        if (providerLow) {
            QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
            float currentFps = recorderHiRes->currentScheduleTask().getFps();

            // second stream should run if camera do not share fps or at least MIN_SECONDARY_FPS frames left for second stream
            bool runSecondStream = (cameraRes->streamFpsSharingMethod() != shareFps || cameraRes->getMaxFps() - currentFps >= MIN_SECONDARY_FPS); 

            if (runSecondStream)
            {
                if (recorderLowRes) {
                    if (!recorderLowRes->isRunning())
                        cl_log.log("Recording started (secondary stream) for camera ", res->getUniqueId(), cl_logINFO);
                    recorderLowRes->start();
                }
                providerLow->start();
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
        camera->stopIfNoActivity();
        /*
        if (needStopHi)
            recorderHiRes->clearUnprocessedData();
        if (needStopLow)
            recorderLowRes->clearUnprocessedData();
        */

        if (needStopHi) {
            cl_log.log("Recording stopped for camera ", res->getUniqueId(), cl_logINFO);
        }
        if(!needStopHi && !needStopLow && res->getStatus() == QnResource::Recording)
            res->setStatus(QnResource::Online); // may be recording thread was not runned, so reset status to online

    }
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

void QnRecordingManager::at_initAsyncFinished(QnResourcePtr res, bool state)
{
    QnVirtualCameraResourcePtr camera = res.dynamicCast<QnVirtualCameraResource>();
    if (camera && state)
        updateCamera(camera);
}

void QnRecordingManager::at_cameraUpdated()
{
    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource> (dynamic_cast<QnVirtualCameraResource*>(sender())->toSharedPointer());
    if (camera) {
        if (!camera->isInitialized() && !camera->isDisabled()) {
            camera->initAsync();
        }

        updateCamera(camera);

        QMap<QnResourcePtr, Recorders>::iterator itr = m_recordMap.find(camera); // && m_recordMap.value(camera).recorderHiRes->isRunning();
        if (itr != m_recordMap.end()) 
        {
            if (itr->recorderHiRes && itr->recorderHiRes->isAudioPresent() != camera->isAudioEnabled()) 
                itr->recorderHiRes->setNeedReopen();
            if (itr->recorderLowRes && itr->recorderLowRes->isAudioPresent() != camera->isAudioEnabled()) 
                itr->recorderLowRes->setNeedReopen();
        }
    }
}

void QnRecordingManager::at_cameraStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    if ((oldStatus == QnResource::Offline || oldStatus == QnResource::Unauthorized) && newStatus == QnResource::Online)
    {
        QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource> (dynamic_cast<QnSecurityCamResource*>(sender())->toSharedPointer());
        if (camera)
            updateCamera(camera);
    }
}

void QnRecordingManager::onNewResource(QnResourcePtr res)
{
    QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (camera) {
        connect(camera.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(at_cameraStatusChanged(QnResource::Status, QnResource::Status)));
        connect(camera.data(), SIGNAL(resourceChanged()), this, SLOT(at_cameraUpdated()));
        connect(camera.data(), SIGNAL(initAsyncFinished(QnResourcePtr, bool)),                this, SLOT(at_initAsyncFinished(QnResourcePtr, bool)));
        updateCamera(camera);
        return;
    }

    QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource>(res);
    if (mediaServer) {
        connect(mediaServer.data(), SIGNAL(resourceChanged()), this, SLOT(at_updateStorage()), Qt::QueuedConnection);
    }
}

void QnRecordingManager::at_updateStorage()
{
    QnMediaServerResource* mediaServer = dynamic_cast<QnMediaServerResource*> (sender());
    qnStorageMan->removeAbsentStorages(mediaServer->getStorages());
    foreach(QnAbstractStorageResourcePtr storage, mediaServer->getStorages())
    {
        QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(storage);
        if (physicalStorage)
            qnStorageMan->addStorage(physicalStorage);
    }
}

void QnRecordingManager::onRemoveResource(QnResourcePtr res)
{
    QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(res);
    if (physicalStorage) {
        qnStorageMan->removeStorage(physicalStorage);
        return;
    }

    Recorders recorders;
    {
        QMutexLocker lock(&m_mutex);
        QMap<QnResourcePtr, Recorders>::const_iterator itr = m_recordMap.find(res);
        if (itr == m_recordMap.constEnd())
            return;
        recorders = itr.value();
        m_recordMap.remove(res);
    }
    qnCameraPool->removeVideoCamera(res);

    beforeDeleteRecorder(recorders);
    deleteRecorder(recorders);
}

bool QnRecordingManager::isCameraRecoring(QnResourcePtr camera)
{
    QMutexLocker lock(&m_mutex);
    return m_recordMap.contains(camera) && m_recordMap.value(camera).recorderHiRes->isRunning();
}

void QnRecordingManager::onTimer()
{
    qint64 time = qnSyncTime->currentMSecsSinceEpoch();
    for (QMap<QnResourcePtr, Recorders>::iterator itrRec = m_recordMap.begin(); itrRec != m_recordMap.end(); ++itrRec)
    {
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(itrRec.key());
        const Recorders& recorders = itrRec.value();

        if (!recorders.recorderHiRes && !recorders.recorderLowRes)
            return; // no recorders are created now

        if (recorders.recorderHiRes)
            recorders.recorderHiRes->updateScheduleInfo(time);
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->updateScheduleInfo(time);
        startOrStopRecording(itrRec.key(), camera, recorders.recorderHiRes, recorders.recorderLowRes);
    }
}


Q_GLOBAL_STATIC(QnRecordingManager, qn_recordingManager_instance);
QnRecordingManager* QnRecordingManager::instance()
{
    return qn_recordingManager_instance();
}

// --------------------- QnServerDataProviderFactory -------------------
Q_GLOBAL_STATIC(QnServerDataProviderFactory, qn_serverDataProviderFactory_instance);

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


