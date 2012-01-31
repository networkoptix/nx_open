#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/camera_resource.h"

const float MIN_SECONDARY_FPS = 2.0;

// ------------------------------ QnVideoCameraGopKeeper --------------------------------

class QnVideoCameraGopKeeper: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    virtual void beforeDisconnectFromResource();
    QnVideoCameraGopKeeper(QnResourcePtr resource);
    virtual ~QnVideoCameraGopKeeper();
    QnAbstractMediaStreamDataProvider* getLiveReader();

    void copyLastGop(CLDataQueue& dstQueue);

    // QnAbstractDataConsumer
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual bool processData(QnAbstractDataPacketPtr data);
private:
    QMutex m_queueMtx;
};

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnResourcePtr resource): 
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100)
{
}

QnVideoCameraGopKeeper::~QnVideoCameraGopKeeper()
{
    stop();
}

void QnVideoCameraGopKeeper::beforeDisconnectFromResource()
{
    pleaseStop();
}

bool QnVideoCameraGopKeeper::canAcceptData() const
{
    return true;
}

void QnVideoCameraGopKeeper::putData(QnAbstractDataPacketPtr data)
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

bool QnVideoCameraGopKeeper::processData(QnAbstractDataPacketPtr /*data*/)
{
    return true;
}

void QnVideoCameraGopKeeper::copyLastGop(CLDataQueue& dstQueue)
{
    QMutexLocker lock(&m_queueMtx);
    for (int i = 0; i < m_dataQueue.size(); ++i)
        dstQueue.push(m_dataQueue.at(i));
}

// --------------- QnVideoCamera ----------------------------

QnVideoCamera::QnVideoCamera(QnResourcePtr resource): m_resource(resource)
{
    m_primaryReader = 0;
    m_secondaryReader = 0;
    m_primaryGopKeeper = 0;
    m_secondaryGopKeeper = 0;
}

void QnVideoCamera::beforeStop()
{
    if (m_primaryGopKeeper)
        m_primaryGopKeeper->beforeDisconnectFromResource();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->beforeDisconnectFromResource();

    if (m_primaryReader)
        m_primaryReader->pleaseStop();

    if (m_secondaryReader)
        m_secondaryReader->pleaseStop();
}

QnVideoCamera::~QnVideoCamera()
{
    beforeStop();

    delete m_primaryGopKeeper;
    delete m_secondaryGopKeeper;

    delete m_primaryReader;
    delete m_secondaryReader;
}

QnAbstractMediaStreamDataProvider* QnVideoCamera::getLiveReader(QnResource::ConnectionRole role)
{
    bool primaryLiveStream = role ==  QnResource::Role_LiveVideo;
    QnAbstractMediaStreamDataProvider* &reader = primaryLiveStream ? m_primaryReader : m_secondaryReader;
    if (!reader) 
    {
        QnAbstractStreamDataProvider* p = m_resource->createDataProvider(role);
        reader = dynamic_cast<QnAbstractMediaStreamDataProvider*> (p);
        QnVideoCameraGopKeeper* gopKeeper = new QnVideoCameraGopKeeper(m_resource);
        if (primaryLiveStream)
            m_primaryGopKeeper = gopKeeper;
        else
            m_secondaryGopKeeper = gopKeeper;
        reader->addDataProcessor(gopKeeper);
        reader->start();

        if (primaryLiveStream) 
        {
            connect(reader, SIGNAL(onFpsChanged(QnAbstractMediaStreamDataProvider*, float)), this, SLOT(onFpsChanged(QnAbstractMediaStreamDataProvider*, float)));
            void onFpsChanged(QnAbstractMediaStreamDataProvider* provider, float value);
        }
    }
    return reader;
}

void QnVideoCamera::copyLastGop(bool primaryLiveStream, CLDataQueue& dstQueue)
{
    if (primaryLiveStream)
        m_primaryGopKeeper->copyLastGop(dstQueue);
    else
        m_secondaryGopKeeper->copyLastGop(dstQueue);
}

void QnVideoCamera::onFpsChanged(QnAbstractMediaStreamDataProvider* provider, float value)
{
    QnPhysicalCameraResourcePtr cameraRes = qSharedPointerDynamicCast<QnPhysicalCameraResource>(m_resource);
    Q_ASSERT(cameraRes != 0);
    if (provider == m_primaryReader && cameraRes->getMaxFps() - value < MIN_SECONDARY_FPS)
    {
        m_secondaryReader->stop();
    }
}
