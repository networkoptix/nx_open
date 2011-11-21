#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/mediadatapacket.h"

QnVideoCamera::QnVideoCamera(QnResourcePtr resource): 
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100),
    m_reader(0)
{

}

QnVideoCamera::~QnVideoCamera()
{
    delete m_reader;
}

void QnVideoCamera::beforeDisconnectFromResource()
{
    if (m_reader)
        m_reader->pleaseStop();
}

QnAbstractMediaStreamDataProvider* QnVideoCamera::getLiveReader()
{
    if (!m_reader) {
        m_reader = dynamic_cast<QnAbstractMediaStreamDataProvider*> (m_resource->createDataProvider(QnResource::Role_LiveVideo));
        m_reader->setQuality(QnQualityHighest);
        m_reader->addDataProcessor(this);
        m_reader->start();
    }
    return m_reader;
}


bool QnVideoCamera::canAcceptData() const
{
    return true;
}

void QnVideoCamera::putData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return;
    QMutexLocker lock(&m_queueMtx);
    if (media->flags & AV_PKT_FLAG_KEY)
        m_dataQueue.clear();
    if (m_dataQueue.size() < m_dataQueue.maxSize()) {
        media->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        QnAbstractDataConsumer::putData(data);
    }
}

bool QnVideoCamera::processData(QnAbstractDataPacketPtr /*data*/)
{
    return true;
}

void QnVideoCamera::copyLastGop(CLDataQueue& dstQueue)
{
    QMutexLocker lock(&m_queueMtx);
    for (int i = 0; i < m_dataQueue.size(); ++i)
        dstQueue.push(m_dataQueue.at(i));
}
