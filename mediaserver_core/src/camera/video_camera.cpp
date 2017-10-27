#include "video_camera.h"

#include <deque>

#include <utils/common/synctime.h>
#include <utils/common/util.h> /* For getUsecTimer. */
#include <utils/media/frame_info.h>
#include <utils/media/media_stream_cache.h>
#include <utils/memory/cyclic_allocator.h>

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/config.h>
#include <nx/utils/random.h>
#include <core/dataprovider/cpull_media_stream_provider.h>
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource/camera_resource.h>

#include <media_server/settings.h>
#include <streaming/hls/hls_types.h>
#include <api/global_settings.h>

static const qint64 CAMERA_UPDATE_INTERNVAL = 3600 * 1000000ll;
static const qint64 KEEP_IFRAMES_INTERVAL = 1000000ll * 80;
static const qint64 KEEP_IFRAMES_DISTANCE = 1000000ll * 5;
static const qint64 GET_FRAME_MAX_TIME = 1000000ll * 15;
static const qint64 MSEC_PER_SEC = 1000;
static const qint64 USEC_PER_MSEC = 1000;
static unsigned int MEDIA_CACHE_SIZE_MILLIS = 10 * MSEC_PER_SEC;
static const int CAMERA_PULLING_STOP_TIMEOUT = 1000 * 3;

// ------------------------------ QnVideoCameraGopKeeper --------------------------------

class QnVideoCameraGopKeeper: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    QnVideoCameraGopKeeper(QnVideoCamera* camera, const QnResourcePtr &resource,  QnServer::ChunksCatalog catalog);
    virtual ~QnVideoCameraGopKeeper();

    virtual void beforeDisconnectFromResource();

    int copyLastGop(qint64 skipTime, QnDataPacketQueue& dstQueue, int cseq, bool iFramesOnly);

    virtual bool canAcceptData() const;
    virtual void putData(const QnAbstractDataPacketPtr& data);
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    QnConstCompressedVideoDataPtr getLastVideoFrame(int channel) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame() const;

    QnConstCompressedVideoDataPtr getIframeByTimeUnsafe(
        qint64 time,
        int channel,
        QnThumbnailRequestData::RoundMethod roundMethod) const;

    QnConstCompressedVideoDataPtr getIframeByTime(
        qint64 time,
        int channel,
        QnThumbnailRequestData::RoundMethod roundMethod) const;

    std::unique_ptr<QnConstDataPacketQueue> getGopTillTime(qint64 time, int channel) const;

    void updateCameraActivity();
    virtual bool needConfigureProvider() const override { return false; }
    void clearVideoData();

private:
    mutable QnMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnConstCompressedAudioDataPtr m_lastAudioData;
    QnConstCompressedVideoDataPtr m_lastKeyFrame[CL_MAX_CHANNELS];
    std::deque<QnConstCompressedVideoDataPtr> m_lastKeyFrames[CL_MAX_CHANNELS];
    int m_gotIFramesMask;
    int m_allChannelsMask;
    bool m_isSecondaryStream;
    QnVideoCamera* m_camera;
    bool m_activityStarted;
    qint64 m_activityStartTime;
    QnServer::ChunksCatalog m_catalog;
    qint64 m_nextMinTryTime;
};

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnVideoCamera* camera, const QnResourcePtr &resource, QnServer::ChunksCatalog catalog):
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100),
    m_lastKeyFrameChannel(0),
    m_gotIFramesMask(0),
    m_allChannelsMask(0),
    m_camera(camera),
    m_activityStarted(false),
    m_activityStartTime(0),
    m_catalog(catalog),
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

void QnVideoCameraGopKeeper::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    QnMutexLocker lock( &m_queueMtx );
    if (QnConstCompressedVideoDataPtr video = std::dynamic_pointer_cast<const QnCompressedVideoData>(nonConstData))
    {
        if (video->flags & AV_PKT_FLAG_KEY)
        {
            if (m_gotIFramesMask == m_allChannelsMask)
            {
                m_dataQueue.clear();
                m_gotIFramesMask = 0;
            }
            m_gotIFramesMask |= 1 << video->channelNumber;
            int ch = video->channelNumber;
            m_lastKeyFrame[ch] = video;
            const qint64 removeThreshold = video->timestamp - KEEP_IFRAMES_INTERVAL;

            if (m_lastKeyFrames[ch].empty() || m_lastKeyFrames[ch].back()->timestamp <= video->timestamp - KEEP_IFRAMES_DISTANCE)
                m_lastKeyFrames[ch].push_back(QnCompressedVideoDataPtr(video->clone(QnSystemAllocator::instance())));

            while ((!m_lastKeyFrames[ch].empty() && m_lastKeyFrames[ch].front()->timestamp < removeThreshold) ||
                    (m_lastKeyFrames[ch].size() > KEEP_IFRAMES_INTERVAL/KEEP_IFRAMES_DISTANCE))
                m_lastKeyFrames[ch].pop_front();
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize()) {
            //TODO #ak MUST NOT modify video packet here! It can be used by other threads concurrently and flags value can be undefined in other threads
            static_cast<QnAbstractMediaData*>(nonConstData.get())->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData( nonConstData );
        }
    }
    else if (QnConstCompressedAudioDataPtr audio = std::dynamic_pointer_cast<const QnCompressedAudioData>(nonConstData))
    {
        m_lastAudioData = std::move(audio);
    }
}

bool QnVideoCameraGopKeeper::processData(const QnAbstractDataPacketPtr& /*data*/)
{
    return true;
}

int QnVideoCameraGopKeeper::copyLastGop(qint64 skipTime, QnDataPacketQueue& dstQueue, int cseq, bool iFramesOnly)
{
    auto addData =
        [&](const QnConstAbstractDataPacketPtr& data)
        {
            const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(data.get());
            if (video)
            {
                QnCompressedVideoData* newData = video->clone();
                if (skipTime && video->timestamp <= skipTime)
                    newData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
                newData->opaque = cseq;
                dstQueue.push(QnAbstractMediaDataPtr(newData));
            }
            else {
                dstQueue.push(std::const_pointer_cast<QnAbstractDataPacket>(data)); //TODO: #ak remove const_cast
            }
        };

    int rez = 0;
    if (iFramesOnly)
    {
        QnMutexLocker lock(&m_queueMtx);
        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        {
            if (m_lastKeyFrame[i])
            {
                addData(m_lastKeyFrame[i]);
                ++rez;
            }
        }
    }
    else
    {
        auto randomAccess = m_dataQueue.lock();
        for (int i = 0; i < randomAccess.size(); ++i)
        {
            addData(randomAccess.at(i));
            ++rez;
        }
    }
    return rez;
}

QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getIframeByTimeUnsafe(
    qint64 time,
    int channel,
    QnThumbnailRequestData::RoundMethod roundMethod) const
{
    const auto &queue = m_lastKeyFrames[channel];
    if (queue.empty())
        return QnConstCompressedVideoDataPtr(); // no video data

    auto condition =
        [](const QnConstCompressedVideoDataPtr& data, qint64 time)
        {
            return data->timestamp < time;
        };

    auto itr = std::lower_bound(
        queue.begin(),
        queue.end(),
        time,
        condition);

    if (itr == queue.end())
    {
        const bool returnLastKeyFrame = m_lastKeyFrame[channel]
            && (m_lastKeyFrame[channel]->timestamp <= time
                || roundMethod != QnThumbnailRequestData::RoundMethod::KeyFrameBeforeMethod);

        if (returnLastKeyFrame)
            return m_lastKeyFrame[channel];
        else
            return queue.back();
    }

    const bool returnIframeBeforeTime = itr != queue.begin()
        && (*itr)->timestamp > time
        && roundMethod == QnThumbnailRequestData::RoundMethod::KeyFrameBeforeMethod;

    if (returnIframeBeforeTime)
        --itr; // prefer frame before defined time if no exact match

    return QnConstCompressedVideoDataPtr((*itr)->clone());
}

QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getIframeByTime(
    qint64 time,
    int channel,
    QnThumbnailRequestData::RoundMethod roundMethod) const
{
    QnMutexLocker lock( &m_queueMtx );
    return getIframeByTimeUnsafe(time, channel, roundMethod);
}

std::unique_ptr<QnConstDataPacketQueue> QnVideoCameraGopKeeper::getGopTillTime(qint64 time, int channel) const
{
    QnMutexLocker lock(&m_queueMtx);

    auto frameSequence = std::make_unique<QnConstDataPacketQueue>();
    auto randomAccess = m_dataQueue.lock();
    for (int i = 0; i < randomAccess.size(); ++i)
    {
        const QnConstAbstractDataPacketPtr& data = randomAccess.at(i);
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(data);
        if (video && video->timestamp <= time && video->channelNumber == channel)
            frameSequence->push(video);
    }

	if (frameSequence->isEmpty())
	{
        auto iframe = getIframeByTimeUnsafe(
            time,
            channel,
            QnThumbnailRequestData::RoundMethod::KeyFrameAfterMethod);
       if (iframe)
	       frameSequence->push(iframe);
	}

    return frameSequence;
}

QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getLastVideoFrame(int channel) const
{
    QnMutexLocker lock( &m_queueMtx );
    return m_lastKeyFrame[channel];
}

QnConstCompressedAudioDataPtr QnVideoCameraGopKeeper::getLastAudioFrame() const
{
    QnMutexLocker lock( &m_queueMtx );
    return m_lastAudioData;
}

void QnVideoCameraGopKeeper::updateCameraActivity()
{
    qint64 usecTime = getUsecTimer();
    qint64 lastKeyTime = AV_NOPTS_VALUE;
    {
        QnMutexLocker lock( &m_queueMtx );
        if (m_lastKeyFrame[0])
            lastKeyTime = m_lastKeyFrame[0]->timestamp;
    }
    if (!m_resource->hasFlags(Qn::foreigner) && m_resource->isInitialized() &&
       (lastKeyTime == (qint64)AV_NOPTS_VALUE || qnSyncTime->currentUSecsSinceEpoch() - lastKeyTime > CAMERA_UPDATE_INTERNVAL) &&
        qnGlobalSettings->isAutoUpdateThumbnailsEnabled())
    {
        if (m_nextMinTryTime == 0) // get first screenshot after minor delay
            m_nextMinTryTime = usecTime + nx::utils::random::number(5000, 10000) * 1000ll;

        if (!m_activityStarted && usecTime > m_nextMinTryTime) {
            m_activityStarted = true;
            m_activityStartTime = usecTime;
            m_nextMinTryTime = usecTime + nx::utils::random::number(100, 100 + 60*5) * 1000000ll;
            m_camera->inUse(this);
            QnLiveStreamProviderPtr provider = m_camera->getLiveReader(m_catalog);
            if (provider)
                provider->startIfNotRunning();
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

void QnVideoCameraGopKeeper::clearVideoData()
{
    QnMutexLocker lock( &m_queueMtx );

    for( QnConstCompressedVideoDataPtr& frame: m_lastKeyFrame )
        frame.reset();
    for( auto& lastKeyFramesForChannel: m_lastKeyFrames )
        lastKeyFramesForChannel.clear();
    m_nextMinTryTime = 0;
}

// --------------- QnVideoCamera ----------------------------

QnVideoCamera::QnVideoCamera(const QnResourcePtr& resource)
:
    m_resource(resource),
    m_primaryGopKeeper(nullptr),
    m_secondaryGopKeeper(nullptr),
    m_loStreamHlsInactivityPeriodMS( MSSettings::roSettings()->value(
        nx_ms_conf::HLS_INACTIVITY_PERIOD,
        nx_ms_conf::DEFAULT_HLS_INACTIVITY_PERIOD ).toInt() * MSEC_PER_SEC ),
    m_hiStreamHlsInactivityPeriodMS( MSSettings::roSettings()->value(
        nx_ms_conf::HLS_INACTIVITY_PERIOD,
        nx_ms_conf::DEFAULT_HLS_INACTIVITY_PERIOD ).toInt() * MSEC_PER_SEC )
{
    //ensuring that vectors will not take much memory
    static_assert(
        ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
        "MediaQuality enum suddenly contains too large values: consider changing QnVideoCamera::m_liveCache, QnVideoCamera::m_hlsLivePlaylistManager type" );

    m_liveCache.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_hlsLivePlaylistManager.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_lastActivityTimer.invalidate();
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
        m_secondaryReader->removeDataProcessor(m_secondaryGopKeeper);
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

void QnVideoCamera::at_camera_resourceChanged()
{
	QnMutexLocker lock( &m_getReaderMutex );

	const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
	if ( cameraResource )
	{
		if ( !cameraResource->hasDualStreaming2() && m_secondaryReader )
		{
			if ( m_secondaryReader->isRunning() )
				m_secondaryReader->pleaseStop();
		}

        if( cameraResource->flags() & Qn::foreigner )
        {
            //clearing saved key frames
            if (m_primaryGopKeeper)
                m_primaryGopKeeper->clearVideoData();
            if (m_secondaryGopKeeper)
                m_secondaryGopKeeper->clearVideoData();
        }
    }
}

void QnVideoCamera::createReader(QnServer::ChunksCatalog catalog)
{
    const bool primaryLiveStream = catalog == QnServer::HiQualityCatalog;
    const Qn::ConnectionRole role = primaryLiveStream ? Qn::CR_LiveVideo : Qn::CR_SecondaryLiveVideo;

	const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());

    QnLiveStreamProviderPtr &reader = primaryLiveStream ? m_primaryReader : m_secondaryReader;
    if (reader == 0)
    {
		QnAbstractStreamDataProvider* dataProvider = NULL;
		if ( primaryLiveStream || (cameraResource && cameraResource->hasDualStreaming2()) )
			dataProvider = m_resource->createDataProvider(role);

		if ( dataProvider )
		{
			reader = QnLiveStreamProviderPtr(dynamic_cast<QnLiveStreamProvider*>(dataProvider));
			if (reader == 0)
			{
				delete dataProvider;
			} else
			{
                reader->setOwner(toSharedPointer());
				if ( role ==  Qn::CR_LiveVideo )
					connect(reader->getResource().data(), SIGNAL(resourceChanged(const QnResourcePtr &)), this, SLOT(at_camera_resourceChanged()), Qt::DirectConnection);

				QnVideoCameraGopKeeper* gopKeeper = new QnVideoCameraGopKeeper(this, m_resource, catalog);
				if (primaryLiveStream)
					m_primaryGopKeeper = gopKeeper;
				else
					m_secondaryGopKeeper = gopKeeper;
				reader->addDataProcessor(gopKeeper);
            }
		}
    }
}

void QnVideoCamera::startLiveCacheIfNeeded()
{
    using namespace std::chrono;

    QnMutexLocker lock(&m_getReaderMutex);

    if (!isSomeActivity())
        return;

    const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
    if (m_secondaryReader)
    {
        if (!m_liveCache[MEDIA_Quality_Low])
            ensureLiveCacheStarted(
                MEDIA_Quality_Low,
                m_secondaryReader,
                duration_cast<microseconds>(MSSettings::hlsTargetDuration()).count());
    }
    else if (m_primaryReader && !m_liveCache[MEDIA_Quality_High])
    {
        //if camera has one stream only and it is suitable for motion then caching first stream
        if (!cameraResource->hasDualStreaming2() &&
            cameraResource->hasCameraCapabilities(Qn::PrimaryStreamSoftMotionCapability))
        {
            ensureLiveCacheStarted(
                MEDIA_Quality_High,
                m_primaryReader,
                duration_cast<microseconds>(MSSettings::hlsTargetDuration()).count());
        }
    }
}

QnLiveStreamProviderPtr QnVideoCamera::getPrimaryReader()
{
    return getLiveReader(QnServer::HiQualityCatalog);
}

QnLiveStreamProviderPtr QnVideoCamera::getSecondaryReader()
{
    return getLiveReader(QnServer::LowQualityCatalog);
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReader(QnServer::ChunksCatalog catalog)
{
    QnMutexLocker lock( &m_getReaderMutex );
    return getLiveReaderNonSafe( catalog );
}

int QnVideoCamera::copyLastGop(
    bool primaryLiveStream,
    qint64 skipTime,
    QnDataPacketQueue& dstQueue,
    int cseq,
    bool iFramesOnly)
{
    if (primaryLiveStream && m_primaryGopKeeper)
        return m_primaryGopKeeper->copyLastGop(skipTime, dstQueue, cseq, iFramesOnly);
    else if (m_secondaryGopKeeper)
        return m_secondaryGopKeeper->copyLastGop(skipTime, dstQueue, cseq, iFramesOnly);
    return 0;
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

std::unique_ptr<QnConstDataPacketQueue> QnVideoCamera::getFrameSequenceByTime(
    bool primaryLiveStream,
    qint64 time,
    int channel,
    QnThumbnailRequestData::RoundMethod roundMethod) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream
        ? m_primaryGopKeeper
        : m_secondaryGopKeeper;

    if (gopKeeper)
    {
        if (roundMethod == QnThumbnailRequestData::RoundMethod::PreciseMethod)
            return gopKeeper->getGopTillTime(time, channel);

        auto frame = gopKeeper->getIframeByTime(time, channel, roundMethod);
        if (frame)
        {
            if (roundMethod == QnThumbnailRequestData::KeyFrameAfterMethod
                && frame->timestamp < time)
            {
                // After frame not found. Return preceise frame instead.
                return gopKeeper->getGopTillTime(time, channel);
            }

            auto queue = std::make_unique<QnConstDataPacketQueue>();
            queue->push(frame);

            return queue;
        }
    }

    return nullptr;
}

QnConstCompressedVideoDataPtr QnVideoCamera::getLastVideoFrame(bool primaryLiveStream, int channel) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastVideoFrame(channel);
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
    QnMutexLocker lock( &m_getReaderMutex );
    m_cameraUsers << user;
    m_lastActivityTimer.restart();
}

void QnVideoCamera::notInUse(void* user)
{
    QnMutexLocker lock( &m_getReaderMutex );
    m_cameraUsers.remove(user);
    m_lastActivityTimer.restart();
}

bool QnVideoCamera::isSomeActivity() const
{
    return !m_cameraUsers.isEmpty() && !m_resource->hasFlags(Qn::foreigner);
}

void QnVideoCamera::updateActivity()
{
    //this method is called periodically

    if (m_primaryGopKeeper)
        m_primaryGopKeeper->updateCameraActivity();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->updateCameraActivity();
    stopIfNoActivity();
    startLiveCacheIfNeeded();
}

void QnVideoCamera::stopIfNoActivity()
{
    QnMutexLocker lock( &m_getReaderMutex );

    //stopping live cache (used for HLS)
    if( (m_liveCache[MEDIA_Quality_High] || m_liveCache[MEDIA_Quality_Low])     //has live cache ever been started?
        &&
        (!m_liveCache[MEDIA_Quality_High] ||                                    //has hi quality live cache been started?
            (m_hlsLivePlaylistManager[MEDIA_Quality_High].unique() &&           //no one uses playlist
             m_hlsLivePlaylistManager[MEDIA_Quality_High]->inactivityPeriod() > m_hiStreamHlsInactivityPeriodMS &&  //checking inactivity timer
             m_liveCache[MEDIA_Quality_High]->inactivityPeriod() > m_hiStreamHlsInactivityPeriodMS))
        &&
        (!m_liveCache[MEDIA_Quality_Low] ||
            (m_hlsLivePlaylistManager[MEDIA_Quality_Low].unique() &&
             m_hlsLivePlaylistManager[MEDIA_Quality_Low]->inactivityPeriod() > m_loStreamHlsInactivityPeriodMS &&
             m_liveCache[MEDIA_Quality_Low]->inactivityPeriod() > m_loStreamHlsInactivityPeriodMS)) )
    {
        m_cameraUsers.remove(this);

        if( m_liveCache[MEDIA_Quality_High] )
        {
            //if single stream is available and that stream is suitable for motion
            //  then stopping caching only if no one else fetches stream
            const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
            const bool isSingleStreamCameraAndHiSuitableForCaching =
                cameraResource &&
                !cameraResource->hasDualStreaming2() &&
                cameraResource->hasCameraCapabilities( Qn::PrimaryStreamSoftMotionCapability );
            if( !isSingleStreamCameraAndHiSuitableForCaching || !isSomeActivity() )
            {
                if( m_primaryReader )
                    m_primaryReader->removeDataProcessor( m_liveCache[MEDIA_Quality_High].get() );
                m_hlsLivePlaylistManager[MEDIA_Quality_High].reset();
                m_liveCache[MEDIA_Quality_High].reset();
            }
        }

        if( m_liveCache[MEDIA_Quality_Low] &&
            !isSomeActivity() )             //stopping low quality stream caching only if no one else fetches stream (i.e., no recording enabled)
        {
            if( m_secondaryReader )
                m_secondaryReader->removeDataProcessor( m_liveCache[MEDIA_Quality_Low].get() );
            m_hlsLivePlaylistManager[MEDIA_Quality_Low].reset();
            m_liveCache[MEDIA_Quality_Low].reset();
        }
    }

    if (isSomeActivity())
        return;
	else if (m_lastActivityTimer.isValid() && m_lastActivityTimer.elapsed() < CAMERA_PULLING_STOP_TIMEOUT)
        return;

    const bool needStopPrimary = m_primaryReader && m_primaryReader->isRunning();
    const bool needStopSecondary = m_secondaryReader && m_secondaryReader->isRunning();

    if (needStopPrimary)
        m_primaryReader->pleaseStop();

    if (needStopSecondary)
        m_secondaryReader->pleaseStop();
}

const MediaStreamCache* QnVideoCamera::liveCache( MediaQuality streamQuality ) const
{
    return streamQuality < m_liveCache.size()
        ? m_liveCache[streamQuality].get()
        : nullptr;
}

MediaStreamCache* QnVideoCamera::liveCache( MediaQuality streamQuality )
{
    return streamQuality < m_liveCache.size()
        ? m_liveCache[streamQuality].get()
        : nullptr;
}

nx_hls::HLSLivePlaylistManagerPtr QnVideoCamera::hlsLivePlaylistManager( MediaQuality streamQuality ) const
{
    return streamQuality < m_hlsLivePlaylistManager.size()
        ? m_hlsLivePlaylistManager[streamQuality]
        : nx_hls::HLSLivePlaylistManagerPtr();
}

//!Starts caching live stream, if not started
/*!
    \return true, if started, false if failed to start
*/
bool QnVideoCamera::ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec )
{
    QnMutexLocker lock( &m_getReaderMutex );

    NX_ASSERT( streamQuality == MEDIA_Quality_High || streamQuality == MEDIA_Quality_Low );

    if( streamQuality == MEDIA_Quality_High )
    {
        if( !m_primaryReader )
            getLiveReaderNonSafe( QnServer::HiQualityCatalog );
        if( !m_primaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_primaryReader, targetDurationUSec );
    }

    if( streamQuality == MEDIA_Quality_Low )
    {
        if( !m_secondaryReader )
            getLiveReaderNonSafe( QnServer::LowQualityCatalog );
        if( !m_secondaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_secondaryReader, targetDurationUSec );
    }

    return false;
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReaderNonSafe(QnServer::ChunksCatalog catalog)
{
    if( m_resource->hasFlags(Qn::foreigner) )
        return QnLiveStreamProviderPtr();

    if( m_resource->isInitialized() )
    {
        if( (catalog == QnServer::HiQualityCatalog && m_primaryReader == 0) ||
            (catalog == QnServer::LowQualityCatalog && m_secondaryReader == 0) )
        {
            createReader(catalog);
        }
    }
    else
    {
        m_resource->initAsync( true );
    }
	const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
	if ( cameraResource && !cameraResource->hasDualStreaming2() && catalog == QnServer::LowQualityCatalog )
		return QnLiveStreamProviderPtr();
    return catalog == QnServer::HiQualityCatalog ? m_primaryReader : m_secondaryReader;
}

//!Starts caching live stream, if not started
/*!
    \return true, if started, false if failed to start
*/
bool QnVideoCamera::ensureLiveCacheStarted(
    MediaQuality streamQuality,
    const QnLiveStreamProviderPtr& primaryReader,
    qint64 targetDurationUSec )
{
    primaryReader->startIfNotRunning();

    m_cameraUsers.insert(this);

    if( !m_liveCache[streamQuality].get() )
    {
        m_liveCache[streamQuality].reset( new MediaStreamCache(
            MEDIA_CACHE_SIZE_MILLIS,
            MEDIA_CACHE_SIZE_MILLIS*10) );  //hls spec requires 7 last chunks to be in memory, adding extra 3 just for case

        int removedChunksToKeepCount = MSSettings::roSettings()->value(
            nx_ms_conf::HLS_REMOVED_LIVE_CHUNKS_TO_KEEP,
            nx_ms_conf::DEFAULT_HLS_REMOVED_LIVE_CHUNKS_TO_KEEP).toInt();


        m_hlsLivePlaylistManager[streamQuality] =
            std::make_shared<nx_hls::HLSLivePlaylistManager>(
                m_liveCache[streamQuality].get(),
                targetDurationUSec,
                removedChunksToKeepCount);
    }
    //connecting live cache to reader
    primaryReader->addDataProcessor( m_liveCache[streamQuality].get() );
    return true;
}
