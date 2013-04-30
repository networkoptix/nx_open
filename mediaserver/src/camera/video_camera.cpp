#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/media_data_packet.h"
#include "core/resource/camera_resource.h"
#include "core/dataprovider/cpull_media_stream_provider.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"
#include "utils/common/synctime.h"

static const qint64 CAMERA_UPDATE_INTERNVAL = 3600 * 1000000ll;

// ------------------------------ QnVideoCameraGopKeeper --------------------------------

class QnVideoCameraGopKeeper: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    virtual void beforeDisconnectFromResource();
    QnVideoCameraGopKeeper(QnVideoCamera* camera, QnResourcePtr resource, QnResource::ConnectionRole role);
    virtual ~QnVideoCameraGopKeeper();
    QnAbstractMediaStreamDataProvider* getLiveReader();

    int copyLastGop(qint64 skipTime, CLDataQueue& dstQueue);

    // QnAbstractDataConsumer
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual bool processData(QnAbstractDataPacketPtr data);

    //QnMediaContextPtr getVideoCodecContext();
    //QnMediaContextPtr getAudioCodecContext();
    QnCompressedVideoDataPtr getLastVideoFrame();
    QnCompressedAudioDataPtr getLastAudioFrame();
    void updateCameraActivity();
private:
    QMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnCompressedAudioDataPtr m_lastAudioData;
    QnCompressedVideoDataPtr m_lastKeyFrame;
    int m_gotIFramesMask;
    int m_allChannelsMask;
    bool m_isSecondaryStream;
    QnVideoCamera* m_camera;
    bool m_activityStarted;
    QnResource::ConnectionRole m_role;
};

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnVideoCamera* camera, QnResourcePtr resource, QnResource::ConnectionRole role): 
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100),
    m_lastKeyFrameChannel(0),
    m_gotIFramesMask(0),
    m_allChannelsMask(0),
    m_camera(camera),
    m_activityStarted(false),
    m_role(role)
{
    const QnResourceVideoLayout* layout = (qSharedPointerDynamicCast<QnMediaResource>(resource))->getVideoLayout();
    m_allChannelsMask = (1 << layout->numberOfChannels()) - 1;
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

bool channelCheckFunctor(const QnAbstractDataPacketPtr& data, QVariant channelNumber)
{
    quint32 ch = channelNumber.toUInt();
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    return media && media->channelNumber == ch;
}

void QnVideoCameraGopKeeper::putData(QnAbstractDataPacketPtr data)
{
    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
    QnCompressedAudioDataPtr audio = qSharedPointerDynamicCast<QnCompressedAudioData>(data);

    QMutexLocker lock(&m_queueMtx);
    if (video)
    {
        /*
        if (video->flags & AV_PKT_FLAG_KEY) {
            m_lastKeyFrameChannel = video->channelNumber;
            m_dataQueue.removeDataByCondition(channelCheckFunctor, video->channelNumber);
            m_lastKeyFrame = video;
        }
        */
        if (video->flags & AV_PKT_FLAG_KEY)
        {
            if (m_gotIFramesMask == m_allChannelsMask)
            {
                m_dataQueue.clear();
                m_gotIFramesMask = 0;
            }
            m_gotIFramesMask |= 1 << video->channelNumber;
            m_lastKeyFrame = video;
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize()) {
            video->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData(video);
        }
    }
    else if (audio) {
        m_lastAudioData = audio;
    }
}

bool QnVideoCameraGopKeeper::processData(QnAbstractDataPacketPtr /*data*/)
{
    return true;
}

int QnVideoCameraGopKeeper::copyLastGop(qint64 skipTime, CLDataQueue& dstQueue)
{
    int rez = 0;
    QMutexLocker lock(&m_queueMtx);
    for (int i = 0; i < m_dataQueue.size(); ++i)
    {
        QnAbstractDataPacketPtr data = m_dataQueue.at(i);
        QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
        if (video && skipTime && video->timestamp <= skipTime)
        {
            QnCompressedVideoData* newData = video->clone();
            newData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
            dstQueue.push(QnAbstractMediaDataPtr(newData));
        }
        else { 
            dstQueue.push(data);
        }
        rez++;
    }
    return rez;
}


QnCompressedVideoDataPtr QnVideoCameraGopKeeper::getLastVideoFrame()
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastKeyFrame;
}

QnCompressedAudioDataPtr QnVideoCameraGopKeeper::getLastAudioFrame()
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastAudioData;
}

void QnVideoCameraGopKeeper::updateCameraActivity()
{
    if (!m_resource->isDisabled() && (!m_lastKeyFrame || qnSyncTime->currentUSecsSinceEpoch() - m_lastKeyFrame->timestamp > CAMERA_UPDATE_INTERNVAL))
    {
        if (!m_activityStarted) {
            m_activityStarted = true;
            m_camera->inUse(this);
            QnAbstractMediaStreamDataProviderPtr provider = m_camera->getLiveReader(m_role);
            if (provider)
                provider->start();
        }
    }
    else {
        if (m_activityStarted) {
            m_activityStarted = false;
            m_camera->notInUse(this);
        }
    }
    
}

// --------------- QnVideoCamera ----------------------------

QnVideoCamera::QnVideoCamera(QnResourcePtr resource): m_resource(resource)
{
    m_primaryGopKeeper = 0;
    m_secondaryGopKeeper = 0;
}

void QnVideoCamera::beforeStop()
{
    if (m_primaryGopKeeper)
        m_primaryGopKeeper->beforeDisconnectFromResource();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->beforeDisconnectFromResource();

    if (m_primaryGopKeeper)
        m_primaryGopKeeper->disconnectFromResource();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->disconnectFromResource();

    if (m_primaryReader) {
        m_primaryReader->removeDataProcessor(m_primaryGopKeeper);
        m_primaryReader->pleaseStop();
    }

    if (m_secondaryReader) {
        m_secondaryReader->removeDataProcessor(m_primaryGopKeeper);
        m_secondaryReader->pleaseStop();
    }

    if (m_primaryReader)
        m_primaryReader->stop();
    if (m_secondaryReader)
        m_secondaryReader->stop();
}

QnVideoCamera::~QnVideoCamera()
{
    beforeStop();
    delete m_primaryGopKeeper;
    delete m_secondaryGopKeeper;
}

void QnVideoCamera::createReader(QnResource::ConnectionRole role)
{
    bool primaryLiveStream = role ==  QnResource::Role_LiveVideo;
    QnAbstractMediaStreamDataProviderPtr &reader = primaryLiveStream ? m_primaryReader : m_secondaryReader;
    if (reader == 0)
    {
        reader = QnAbstractMediaStreamDataProviderPtr(dynamic_cast<QnAbstractMediaStreamDataProvider*> (m_resource->createDataProvider(role)));
        if (reader == 0)
            return;

        QnVideoCameraGopKeeper* gopKeeper = new QnVideoCameraGopKeeper(this, m_resource, role);
        if (primaryLiveStream)
            m_primaryGopKeeper = gopKeeper;
        else
            m_secondaryGopKeeper = gopKeeper;
        reader->addDataProcessor(gopKeeper);

    }
}

QnAbstractMediaStreamDataProviderPtr QnVideoCamera::getLiveReader(QnResource::ConnectionRole role)
{
    QMutexLocker lock(&m_getReaderMutex);

    if (m_primaryReader == 0 && !m_resource->isDisabled() && m_resource->isInitialized())
    {
        createReader(QnResource::Role_LiveVideo);
        createReader(QnResource::Role_SecondaryLiveVideo);
        //if (m_primaryReader)
        //    m_primaryReader->start();
        //if (m_secondaryReader)
        //    m_secondaryReader->start();
    }
    return role == QnResource::Role_LiveVideo ? m_primaryReader : m_secondaryReader;
}

int QnVideoCamera::copyLastGop(bool primaryLiveStream, qint64 skipTime, CLDataQueue& dstQueue)
{
    if (primaryLiveStream)
        return m_primaryGopKeeper->copyLastGop(skipTime, dstQueue);
    else
        return m_secondaryGopKeeper->copyLastGop(skipTime, dstQueue);
}

/*
QnMediaContextPtr QnVideoCamera::getVideoCodecContext(bool primaryLiveStream)
{
    if (primaryLiveStream)
        return m_primaryGopKeeper->getVideoCodecContext();
    else
        return m_secondaryGopKeeper->getVideoCodecContext();
}

QnMediaContextPtr QnVideoCamera::getAudioCodecContext(bool primaryLiveStream)
{
    if (primaryLiveStream)
        return m_primaryGopKeeper->getAudioCodecContext();
    else
        return m_secondaryGopKeeper->getAudioCodecContext();
}
*/


QnCompressedVideoDataPtr QnVideoCamera::getLastVideoFrame(bool primaryLiveStream)
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastVideoFrame();
    else
        return QnCompressedVideoDataPtr();
}

QnCompressedAudioDataPtr QnVideoCamera::getLastAudioFrame(bool primaryLiveStream)
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastAudioFrame();
    else
        return QnCompressedAudioDataPtr();
}

void QnVideoCamera::inUse(void* user)
{
    QMutexLocker lock(&m_getReaderMutex);
    m_cameraUsers << user;
}

void QnVideoCamera::notInUse(void* user)
{
    QMutexLocker lock(&m_getReaderMutex);
    m_cameraUsers.remove(user);
}

bool QnVideoCamera::isSomeActivity() const
{
    return !m_cameraUsers.isEmpty() && !m_resource->isDisabled();
}

void QnVideoCamera::updateActivity()
{
    if (m_primaryGopKeeper)
        m_primaryGopKeeper->updateCameraActivity();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->updateCameraActivity();
    stopIfNoActivity();
};

void QnVideoCamera::stopIfNoActivity()
{
    QMutexLocker lock(&m_getReaderMutex);
    if (isSomeActivity())
        return;

    bool needStopPrimary = m_primaryReader && m_primaryReader->isRunning();
    bool needStopSecondary = m_secondaryReader && m_secondaryReader->isRunning();

    if (needStopPrimary)
        m_primaryReader->pleaseStop();
    if (needStopSecondary)
        m_secondaryReader->pleaseStop();

    /*
    if (needStopPrimary)
        m_primaryReader->stop();
    if (needStopSecondary)
        m_secondaryReader->stop();
    */
}
