#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/media_data_packet.h"
#include "core/resource/camera_resource.h"
#include "core/dataprovider/cpull_media_stream_provider.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

// ------------------------------ QnVideoCameraGopKeeper --------------------------------

class QnVideoCameraGopKeeper: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    virtual void beforeDisconnectFromResource();
    QnVideoCameraGopKeeper(QnResourcePtr resource);
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
private:
    QMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnCompressedAudioDataPtr m_lastAudioData;
    QnCompressedVideoDataPtr m_lastKeyFrame;
    int m_gotIFramesMask;
    int m_allChannelsMask;
};

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnResourcePtr resource): 
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100),
    m_lastKeyFrameChannel(0),
    m_gotIFramesMask(0),
    m_allChannelsMask(0)
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
    int ch = channelNumber.toInt();
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

/*
QnMediaContextPtr QnVideoCameraGopKeeper::getVideoCodecContext()
{
    QnMediaContextPtr rez;
    QMutexLocker lock(&m_queueMtx);
    if (m_lastKeyFrame == 0)
        return rez;
    
    rez = m_lastKeyFrame->context;
    if (rez == 0)
    {
        // context is not filled. determine context by video payload
        CLVideoDecoderOutput outFrame;
        CLFFmpegVideoDecoder decoder(m_lastKeyFrame->compressionType, m_lastKeyFrame, false);
        decoder.decode(m_lastKeyFrame, &outFrame);
        rez = QnMediaContextPtr(new QnMediaContext(decoder.getContext()));
    }

    return rez;
}

QnMediaContextPtr QnVideoCameraGopKeeper::getAudioCodecContext()
{
    return m_lastAudioData->context;
}
*/

int QnVideoCameraGopKeeper::copyLastGop(qint64 skipTime, CLDataQueue& dstQueue)
{
    int rez = 0;
    QMutexLocker lock(&m_queueMtx);
    for (int i = 0; i < m_dataQueue.size(); ++i)
    {
        QnAbstractDataPacketPtr data = m_dataQueue.at(i);
        if (skipTime)
        {
            QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
            if (video && video->timestamp <= skipTime)
                video->flags |= QnAbstractMediaData::MediaFlags_Ignore;
        }
        dstQueue.push(data);
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

        QnVideoCameraGopKeeper* gopKeeper = new QnVideoCameraGopKeeper(m_resource);
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
    if (primaryLiveStream)
        return m_primaryGopKeeper->getLastAudioFrame();
    else
        return m_secondaryGopKeeper->getLastAudioFrame();
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

    if (needStopPrimary)
        m_primaryReader->stop();
    if (needStopSecondary)
        m_secondaryReader->stop();
}
