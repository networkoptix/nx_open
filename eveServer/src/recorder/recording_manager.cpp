#include "recording_manager.h"
#include "core/resourcemanagment/resource_pool.h"

QnRecordingManager::QnRecordingManager()
{
    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr res)), this, SLOT(onNewResource(QnResourcePtr)));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr res)), this, SLOT(onRemoveResource(QnResourcePtr)));
}

void QnRecordingManager::onNewResource(QnResourcePtr res)
{
    /*
    QnSequrityCamResourcePtr camera = qSharedPointerDynamicCast<QnSequrityCamResource>(res);
    if (camera)
    {   
        res->acquireMediaProvider(Provider_Primary);

        m_reader->setQuality(QnQualityHighest);
        if (m_recorder == 0) {
            m_recorder = new QnStreamRecorder(m_device);
            m_recorder->setTruncateInterval(15);
            connect(m_recorder, SIGNAL(recordingFailed(QString)), this, SIGNAL(recordingFailed(QString)));
        }
        m_reader->addDataProcessor(m_recorder);
        m_reader->setNeedKeyData();
        m_recorder->start();
        m_videovindow->addSubItem(RecordingSubItem);
    }
    */
}

void QnRecordingManager::onRemoveResource(QnResourcePtr res)
{

}
