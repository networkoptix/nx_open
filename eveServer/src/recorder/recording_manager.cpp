#include "recording_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resourcemanagment/security_cam_resource.h"
#include "recording/stream_recorder.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "camera/camera_pool.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "plugins/resources/archive/archive_stream_reader.h"

static const int TRUNCATE_INTERVAL = 15; // seconds

QnRecordingManager::QnRecordingManager()
{
}

QnRecordingManager::~QnRecordingManager()
{
}

void QnRecordingManager::start()
{
    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(onNewResource(QnResourcePtr)));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(onRemoveResource(QnResourcePtr)));
}

void QnRecordingManager::onNewResource(QnResourcePtr res)
{
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);
    if (camera) 
    {
        QnSequrityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSequrityCamResource>(res);
        cameraRes->setDataProviderFactory(QnServerDataProviderFactory::instance());
        QnAbstractMediaStreamDataProvider* reader = camera->getLiveReader();
        QnStreamRecorder* recorder = new QnStreamRecorder(res);
        recorder->setTruncateInterval(TRUNCATE_INTERVAL);
        connect(recorder, SIGNAL(recordingFailed(QString)), this, SIGNAL(recordingFailed(QString)));
        reader->addDataProcessor(recorder);
        reader->setNeedKeyData();
        recorder->start();
    }
}

void QnRecordingManager::recordingFailed(QString errMessage)
{

}

void QnRecordingManager::onRemoveResource(QnResourcePtr res)
{

}

// --------------------- QnServerDataProviderFactory -------------------
Q_GLOBAL_STATIC(QnServerDataProviderFactory, inst);

QnAbstractStreamDataProvider* QnServerDataProviderFactory::createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role)
{
    if (role == QnResource::Role_Archive) 
    {
        QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(res);
        archiveReader->setArchiveDelegate(new QnServerArchiveDelegate());
        return archiveReader;
    }
}

QnServerDataProviderFactory* QnServerDataProviderFactory::instance()
{
    return inst();
}
