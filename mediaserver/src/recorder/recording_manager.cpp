#include "recording_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/security_cam_resource.h"
#include "recording/stream_recorder.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "camera/camera_pool.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "camera/video_camera.h"
#include "core/misc/scheduleTask.h"
#include "server_stream_recorder.h"
#include "utils/common/synctime.h"
#include "core/resource/video_server.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/camera_resource.h"

const float MIN_SECONDARY_FPS = 2.0;


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

void QnRecordingManager::stop()
{
    foreach(const Recorders& recorders, m_recordMap.values())
    {
        recorders.recorderHiRes->pleaseStop();
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->pleaseStop();
    }
    foreach(const Recorders& recorders, m_recordMap.values()) 
    {
        recorders.recorderHiRes->stop();
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->stop();
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(recorders.recorderHiRes->getResource());
        if (camera)
        {
            QnAbstractMediaStreamDataProvider* reader = camera->getLiveReader(QnResource::Role_LiveVideo);
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
    m_recordMap.clear();
}

Recorders QnRecordingManager::findRecorders(QnResourcePtr res) const
{
    QMutexLocker lock(&m_mutex);
    return m_recordMap.value(res);
}

QnServerStreamRecorder* QnRecordingManager::createRecorder(QnResourcePtr res, QnVideoCamera* camera, QnResource::ConnectionRole role)
{
    QnAbstractMediaStreamDataProvider* reader = camera->getLiveReader(role);
    if (reader == 0)
        return 0;
    QnServerStreamRecorder* recorder = new QnServerStreamRecorder(res, role, reader);
    recorder->setTruncateInterval(RECORDING_CHUNK_LEN);
    reader->addDataProcessor(recorder);
    reader->setNeedKeyData();
    //recorder->start();
    return recorder;
}

bool QnRecordingManager::isResourceDisabled(QnResourcePtr res) const
{
    if (res->getStatus() == QnResource::Disabled)
        return true;
    QnVirtualCameraResourcePtr cameraRes = qSharedPointerDynamicCast<QnVirtualCameraResource>(res);
    return  cameraRes && cameraRes->isScheduleDisabled();
}

void QnRecordingManager::startOrStopRecording(QnResourcePtr res, QnVideoCamera* camera, QnServerStreamRecorder* recorderHiRes, QnServerStreamRecorder* recorderLowRes)
{
    QnAbstractMediaStreamDataProvider* providerHi = camera->getLiveReader(QnResource::Role_LiveVideo);
    QnAbstractMediaStreamDataProvider* providerLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);

    if (!isResourceDisabled(res) && recorderHiRes->currentScheduleTask().getRecordingType() != QnScheduleTask::RecordingType_Never)
    {
        if (providerHi)
            providerHi->start();

        if (providerLow)
            onFpsChanged(recorderHiRes, recorderHiRes->currentScheduleTask().getFps()); // start or stop secondary recorder

        recorderHiRes->start();
        if (recorderLowRes)
            recorderLowRes->start();
    }
    else 
    {
        bool needStopHi = recorderHiRes && recorderHiRes->isRunning();
        bool needStopLow = recorderLowRes && recorderLowRes->isRunning();

        if (needStopHi)
            recorderHiRes->pleaseStop();
        if (needStopLow)
            recorderLowRes->pleaseStop();

        if (needStopHi)
            recorderHiRes->stop();
        if (needStopLow)
            recorderLowRes->stop();
        camera->stopIfNoActivity();
    }
}

void QnRecordingManager::updateCamera(QnSecurityCamResourcePtr res)
{
    QMutexLocker lock(&m_mutex);
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);
    QnAbstractMediaStreamDataProvider* providerHi = camera->getLiveReader(QnResource::Role_LiveVideo);
    QnAbstractMediaStreamDataProvider* providerLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);

    if (camera)
    {
        QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
        cameraRes->setDataProviderFactory(QnServerDataProviderFactory::instance());

        QMap<QnResourcePtr, Recorders>::iterator itrRec = m_recordMap.find(res);
        if (itrRec != m_recordMap.end())
        {
            const Recorders& recorders = itrRec.value();
            recorders.recorderHiRes->updateCamera(res);
            if (recorders.recorderLowRes)
                recorders.recorderLowRes->updateCamera(res);

            startOrStopRecording(res, camera, recorders.recorderHiRes, recorders.recorderLowRes);
        }
        else if (res->getStatus() != QnResource::Disabled)
        {
            QnServerStreamRecorder* recorderHiRes = createRecorder(res, camera, QnResource::Role_LiveVideo);
            QnServerStreamRecorder* recorderLowRes = createRecorder(res, camera, QnResource::Role_SecondaryLiveVideo);
            if (recorderHiRes)
                connect(recorderHiRes, SIGNAL(fpsChanged(QnServerStreamRecorder*, float)), this, SLOT(onFpsChanged(QnServerStreamRecorder*, float)));
            if (recorderLowRes) 
                connect(camera->getLiveReader(QnResource::Role_SecondaryLiveVideo), SIGNAL(threadPaused()), recorderLowRes, SLOT(closeOnEOF()), Qt::DirectConnection);
            connect(res.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(onResourceStatusChanged(QnResource::Status, QnResource::Status)), Qt::QueuedConnection);

            m_recordMap.insert(res, Recorders(recorderHiRes, recorderLowRes));

            recorderHiRes->updateCamera(cameraRes);
            if (recorderLowRes)
                recorderLowRes->updateCamera(cameraRes);

            startOrStopRecording(res, camera, recorderHiRes, recorderLowRes);
        }
    }
}

void QnRecordingManager::onNewResource(QnResourcePtr res)
{
    QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (camera) {
        updateCamera(camera);
        return;
    }

    QnVideoServerResourcePtr videoServer = qSharedPointerDynamicCast<QnVideoServerResource>(res);
    if (videoServer) {
        connect(videoServer.data(), SIGNAL(resourceChanged()), this, SLOT(at_updateStorage()), Qt::QueuedConnection);
    }
}

void QnRecordingManager::at_updateStorage()
{
    QnVideoServerResource* videoServer = dynamic_cast<QnVideoServerResource*> (sender());
    foreach(QnStorageResourcePtr storage, videoServer->getStorages())
        qnStorageMan->addStorage(storage);
}

void QnRecordingManager::onRemoveResource(QnResourcePtr res)
{
    QMap<QnResourcePtr, Recorders>::iterator itr = m_recordMap.find(res);
    if (itr == m_recordMap.end())
        return;

    qnCameraPool->removeVideoCamera(itr.key());
    const Recorders& recorders = itr.value();
    delete recorders.recorderHiRes;
    delete recorders.recorderLowRes;
    m_recordMap.erase(itr);
}

bool QnRecordingManager::isCameraRecoring(QnResourcePtr camera)
{
    QMutexLocker lock(&m_mutex);
    return m_recordMap.contains(camera) && m_recordMap.value(camera).recorderHiRes->isRunning();
}

void QnRecordingManager::onFpsChanged(QnServerStreamRecorder* recorder, float value)
{
    //QnServerStreamRecorder* recorder = dynamic_cast<QnServerStreamRecorder*>(sender());
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(recorder->getResource());
    if (camera)
    {
        QnAbstractMediaStreamDataProvider* providerLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);
        QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource> (recorder->getResource());
        if (cameraRes && providerLow)
        {
            if (isResourceDisabled(recorder->getResource()) ||
                cameraRes->getMaxFps() - value < MIN_SECONDARY_FPS || 
                recorder->currentScheduleTask().getRecordingType() == QnScheduleTask::RecordingType_Never) 
                providerLow->stop();
            else 
                providerLow->start();
        }
    }
}

void QnRecordingManager::onResourceStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    // Close file if resource is offline
    // todo: Code is not thread safe now. So, while commented
#if 1
    if (newStatus != QnResource::Offline) 
        return;
    QnResource* res = (QnResource*) sender();
    QnResourcePtr resPtr= res->toSharedPointer();
    if (!resPtr)
        return;
    
    QMutexLocker lock(&m_mutex);
    Recorders recorders = m_recordMap.value(resPtr);
    if (recorders.recorderHiRes)
        recorders.recorderHiRes->closeOnEOF();
    if (recorders.recorderLowRes)
        recorders.recorderLowRes->closeOnEOF();
#endif
}

void QnRecordingManager::onTimer()
{
    qint64 time = qnSyncTime->currentMSecsSinceEpoch();
    for (QMap<QnResourcePtr, Recorders>::iterator itrRec = m_recordMap.begin(); itrRec != m_recordMap.end(); ++itrRec)
    {
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(itrRec.key());
        const Recorders& recorders = itrRec.value();
        recorders.recorderHiRes->updateScheduleInfo(time);
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->updateScheduleInfo(time);
        startOrStopRecording(itrRec.key(), camera, recorders.recorderHiRes, recorders.recorderLowRes);
    }
}


Q_GLOBAL_STATIC(QnRecordingManager, inst2);
QnRecordingManager* QnRecordingManager::instance()
{
    return inst2();
}

// --------------------- QnServerDataProviderFactory -------------------
Q_GLOBAL_STATIC(QnServerDataProviderFactory, inst);

QnAbstractStreamDataProvider* QnServerDataProviderFactory::createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role)
{
    if (role == QnResource::Role_Archive)
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
    return inst();
}
