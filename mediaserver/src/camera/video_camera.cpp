#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/media_data_packet.h"
#include "core/resource/camera_resource.h"
#include "core/dataprovider/cpull_media_stream_provider.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"
#include "utils/common/synctime.h"
#include <deque>
#include "core/dataprovider/live_stream_provider.h"

static const qint64 CAMERA_UPDATE_INTERNVAL = 3600 * 1000000ll;
static const qint64 KEEP_IFRAMES_INTERVAL = 1000000ll * 80;
static const qint64 KEEP_IFRAMES_DISTANCE = 1000000ll * 5;
static const qint64 GET_FRAME_MAX_TIME = 1000000ll * 15;

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
    QnConstCompressedVideoDataPtr getLastVideoFrame() const;
    QnConstCompressedVideoDataPtr GetIFrameByTime(qint64 time, bool iFrameAfterTime) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame() const;
    void updateCameraActivity();
private:
    mutable QMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnConstCompressedAudioDataPtr m_lastAudioData;
    QnConstCompressedVideoDataPtr m_lastKeyFrame;
    std::deque<QnConstCompressedVideoDataPtr> m_lastKeyFrames;
    int m_gotIFramesMask;
    int m_allChannelsMask;
    bool m_isSecondaryStream;
    QnVideoCamera* m_camera;
    bool m_activityStarted;
    qint64 m_activityStartTime;
    QnResource::ConnectionRole m_role;
    qint64 m_nextMinTryTime;
};

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnVideoCamera* camera, QnResourcePtr resource, QnResource::ConnectionRole role): 
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100),
    m_lastKeyFrameChannel(0),
    m_gotIFramesMask(0),
    m_allChannelsMask(0),
    m_camera(camera),
    m_activityStarted(false),
    m_activityStartTime(0),
    m_role(role),
    m_nextMinTryTime(0)
{
    QnConstResourceVideoLayoutPtr layout = (qSharedPointerDynamicCast<QnMediaResource>(resource))->getVideoLayout();
    m_allChannelsMask = (1 << layout->channelCount()) - 1;
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

void QnVideoCameraGopKeeper::putData(QnAbstractDataPacketPtr nonConstData)
{
    QnAbstractDataPacketPtr data = nonConstData;

    QnConstCompressedVideoDataPtr video = qSharedPointerDynamicCast<const QnCompressedVideoData>(data);
    QnConstCompressedAudioDataPtr audio = qSharedPointerDynamicCast<const QnCompressedAudioData>(data);

    QMutexLocker lock(&m_queueMtx);
    if (video)
    {
        if (video->flags & AV_PKT_FLAG_KEY)
        {
            if (m_gotIFramesMask == m_allChannelsMask)
            {
                m_dataQueue.clear();
                m_gotIFramesMask = 0;
            }
            m_gotIFramesMask |= 1 << video->channelNumber;
            m_lastKeyFrame = video;
            if (m_lastKeyFrames.empty() || m_lastKeyFrames.back()->timestamp <= video->timestamp - KEEP_IFRAMES_DISTANCE)
                m_lastKeyFrames.push_back(video);
            qint64 removeThreshold = video->timestamp - KEEP_IFRAMES_INTERVAL;
            while (!m_lastKeyFrames.empty() && m_lastKeyFrames.front()->timestamp < removeThreshold)
                m_lastKeyFrames.pop_front();
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize()) {
            video.constCast<QnCompressedVideoData>()->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData(nonConstData);
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
        QnConstAbstractDataPacketPtr data = m_dataQueue.at(i);
        QnConstCompressedVideoDataPtr video = qSharedPointerDynamicCast<const QnCompressedVideoData>(data);
        if (video && skipTime && video->timestamp <= skipTime)
        {
            QnCompressedVideoData* newData = video->clone();
            newData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
            dstQueue.push(QnAbstractMediaDataPtr(newData));
        }
        else { 
            dstQueue.push(data.constCast<QnAbstractDataPacket>());    //TODO: #ak remove const_cast
        }
        rez++;
    }
    return rez;
}


QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::GetIFrameByTime(qint64 time, bool iFrameAfterTime) const
{
    QnConstCompressedVideoDataPtr result;

    QMutexLocker lock(&m_queueMtx);
    if (m_lastKeyFrames.empty() || 
        m_lastKeyFrames.front()->timestamp > time + KEEP_IFRAMES_DISTANCE ||
        m_lastKeyFrames.back()->timestamp < time - KEEP_IFRAMES_DISTANCE)
    {
        return result;
    }

    for (int i = 0; i < (int)m_lastKeyFrames.size(); ++i)
    {
        if (m_lastKeyFrames[i]->timestamp >= time) {
            if (iFrameAfterTime || m_lastKeyFrames[i]->timestamp == time || i == 0)
                return m_lastKeyFrames[i];
            else
                return m_lastKeyFrames[i-1];
        }
    }
    if (iFrameAfterTime || m_lastKeyFrames.empty())
        return m_lastKeyFrame;
    else
        return m_lastKeyFrames.back();
}


QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getLastVideoFrame() const
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastKeyFrame;
}

QnConstCompressedAudioDataPtr QnVideoCameraGopKeeper::getLastAudioFrame() const
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastAudioData;
}

void QnVideoCameraGopKeeper::updateCameraActivity()
{
    qint64 usecTime = getUsecTimer();
    if (!m_resource->isDisabled() && m_resource->isInitialized() &&
       (!m_lastKeyFrame || qnSyncTime->currentUSecsSinceEpoch() - m_lastKeyFrame->timestamp > CAMERA_UPDATE_INTERNVAL))
    {
        if (!m_activityStarted && usecTime > m_nextMinTryTime) {
            m_activityStarted = true;
            m_activityStartTime = usecTime;
            m_nextMinTryTime = usecTime + (rand()%100 + 60*5) * 1000000ll;
            m_camera->inUse(this);
            QnLiveStreamProviderPtr provider = m_camera->getLiveReader(m_role);
            if (provider)
                provider->startIfNotRunning(false);
        }
        else if (m_activityStarted && usecTime - m_activityStartTime > GET_FRAME_MAX_TIME )
        {
            m_activityStarted = false;
            m_camera->notInUse(this);
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
}

void QnVideoCamera::stop()
{
    if (m_primaryReader)
        m_primaryReader->stop();
    if (m_secondaryReader)
        m_secondaryReader->stop();
}


QnVideoCamera::~QnVideoCamera()
{
    beforeStop();
    stop();
    delete m_primaryGopKeeper;
    delete m_secondaryGopKeeper;
}

void QnVideoCamera::createReader(QnResource::ConnectionRole role)
{
    bool primaryLiveStream = role ==  QnResource::Role_LiveVideo;
    QnLiveStreamProviderPtr &reader = primaryLiveStream ? m_primaryReader : m_secondaryReader;
    if (reader == 0)
    {
        QnAbstractStreamDataProvider* dataProvider = m_resource->createDataProvider(role);
        reader = QnLiveStreamProviderPtr(dynamic_cast<QnLiveStreamProvider*>(dataProvider));
        if (reader == 0)
        {
            delete dataProvider;
            return;
        }

        QnVideoCameraGopKeeper* gopKeeper = new QnVideoCameraGopKeeper(this, m_resource, role);
        if (primaryLiveStream)
            m_primaryGopKeeper = gopKeeper;
        else
            m_secondaryGopKeeper = gopKeeper;
        reader->addDataProcessor(gopKeeper);

    }
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReader(QnResource::ConnectionRole role)
{
    QMutexLocker lock(&m_getReaderMutex);

    if (!m_resource->isDisabled() && m_resource->isInitialized()) 
    {
        if (role == QnResource::Role_LiveVideo && m_primaryReader == 0)
            createReader(QnResource::Role_LiveVideo);
        else if (role == QnResource::Role_SecondaryLiveVideo && m_secondaryReader == 0)
            createReader(QnResource::Role_SecondaryLiveVideo);
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

QnConstCompressedVideoDataPtr QnVideoCamera::getFrameByTime(bool primaryLiveStream, qint64 time, bool iFrameAfterTime) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->GetIFrameByTime(time, iFrameAfterTime);
    else
        return QnCompressedVideoDataPtr();
}

QnConstCompressedVideoDataPtr QnVideoCamera::getLastVideoFrame(bool primaryLiveStream) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastVideoFrame();
    else
        return QnCompressedVideoDataPtr();
}

QnConstCompressedAudioDataPtr QnVideoCamera::getLastAudioFrame(bool primaryLiveStream) const
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
