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

static const int TRUNCATE_INTERVAL = 60; // seconds

QnRecordingManager::QnRecordingManager()
{
}

QnRecordingManager::~QnRecordingManager()
{
    stop();
}

void QnRecordingManager::start()
{
    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(onNewResource(QnResourcePtr)));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(onRemoveResource(QnResourcePtr)));
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
    QnServerStreamRecorder* recorder = new QnServerStreamRecorder(res, role);
    recorder->setTruncateInterval(TRUNCATE_INTERVAL);
    reader->addDataProcessor(recorder);
    reader->setNeedKeyData();
    recorder->start();
    return recorder;
}

void QnRecordingManager::onNewResource(QnResourcePtr res)
{
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);
    if (camera)
    {
        QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
        cameraRes->setDataProviderFactory(QnServerDataProviderFactory::instance());

        QnServerStreamRecorder* recorderHiRes = createRecorder(res, camera, QnResource::Role_LiveVideo);
        QnServerStreamRecorder* recorderLowRes = createRecorder(res, camera, QnResource::Role_SecondaryLiveVideo);
        if (recorderLowRes) 
            connect(camera->getLiveReader(QnResource::Role_SecondaryLiveVideo), SIGNAL(threadPaused()), recorderLowRes, SLOT(closeOnEOF()), Qt::DirectConnection);
        

        QMutexLocker lock(&m_mutex);

        m_recordMap.insert(res, Recorders(recorderHiRes, recorderLowRes));

        QMap<QnId, QnScheduleTaskList>::iterator scheduleItr = m_scheduleByCamera.find(res->getId());
        if (scheduleItr != m_scheduleByCamera.end())
            recorderHiRes->updateSchedule(scheduleItr.value());
        if (recorderLowRes && scheduleItr != m_scheduleByCamera.end())
            recorderLowRes->updateSchedule(scheduleItr.value());
    }
}

void QnRecordingManager::updateSchedule(QnSecurityCamResourcePtr camera)
{
    QMutexLocker lock(&m_mutex);

    QMap<QnResourcePtr, Recorders>::iterator itrRec = m_recordMap.find(camera);
    if (itrRec != m_recordMap.end())
    {
        const Recorders& recorders = itrRec.value();
        recorders.recorderHiRes->updateSchedule(camera->getScheduleTasks());
        if (recorders.recorderLowRes)
            recorders.recorderLowRes->updateSchedule(camera->getScheduleTasks());
    }
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
    return m_recordMap.contains(camera);
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
