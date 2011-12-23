#include "recording_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resourcemanagment/security_cam_resource.h"
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
    foreach(QnServerStreamRecorder* recorder, m_recordMap.values())
        recorder->pleaseStop();
    foreach(QnServerStreamRecorder* recorder, m_recordMap.values()) {
        recorder->stop();
        delete recorder;
    }
    m_recordMap.clear();
}

void QnRecordingManager::onNewResource(QnResourcePtr res)
{
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);
    if (camera) 
    {
        QnSequrityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSequrityCamResource>(res);
        cameraRes->setDataProviderFactory(QnServerDataProviderFactory::instance());
        QnAbstractMediaStreamDataProvider* reader = camera->getLiveReader();
        QnServerStreamRecorder* recorder = new QnServerStreamRecorder(res);
        recorder->setTruncateInterval(TRUNCATE_INTERVAL);
        connect(recorder, SIGNAL(recordingFailed(QString)), this, SIGNAL(recordingFailed(QString)));
        reader->addDataProcessor(recorder);
        reader->setNeedKeyData();
        recorder->start();
        m_recordMap.insert(res, recorder);

        QMutexLocker lock(&m_mutex);
        QMap<QnId, QnScheduleTaskList>::iterator scheduleItr = m_scheduleByCamera.find(res->getId());
        if (scheduleItr != m_scheduleByCamera.end())
            recorder->updateSchedule(scheduleItr.value());
    }
}

void QnRecordingManager::updateSchedule(QnSequrityCamResourcePtr camera)
{
    QMutexLocker lock(&m_mutex);

    QMap<QnResourcePtr, QnServerStreamRecorder*>::iterator itrRec = m_recordMap.find(camera);
    if (itrRec != m_recordMap.end())
    {
        QnServerStreamRecorder* recorder = itrRec.value();
        recorder->updateSchedule(camera->getScheduleTasks());
    }
}

void QnRecordingManager::recordingFailed(QString errMessage)
{
    QnServerStreamRecorder* recorder = static_cast<QnServerStreamRecorder*>(sender());
    for (QMap<QnResourcePtr, QnServerStreamRecorder*>::iterator itr = m_recordMap.begin(); itr != m_recordMap.end(); ++itr)
    {
        if (itr.value() == recorder) 
        {
            qnCameraPool->removeVideoCamera(itr.key());
            m_recordMap.erase(itr);
            break;
        }
    }
    delete recorder;
}

void QnRecordingManager::onRemoveResource(QnResourcePtr res)
{
    QMap<QnResourcePtr, QnServerStreamRecorder*>::iterator itr = m_recordMap.find(res);
    if (itr == m_recordMap.end())
        return;
    
    qnCameraPool->removeVideoCamera(itr.key());
    QnServerStreamRecorder* recorder = static_cast<QnServerStreamRecorder*>(itr.value());
    m_recordMap.erase(itr);
    delete recorder;
}

bool QnRecordingManager::isCameraRecoring(QnResourcePtr camera)
{
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
