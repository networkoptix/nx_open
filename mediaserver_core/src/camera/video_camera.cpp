#include "video_camera.h"

#include <deque>

#include <utils/common/synctime.h>
#include <utils/common/util.h> /* For getUsecTimer. */
#include <utils/media/frame_info.h>
#include <utils/media/media_stream_cache.h>
#include <utils/memory/cyclic_allocator.h>

#include <nx/network/hls/hls_types.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/config.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <providers/cpull_media_stream_provider.h>
#include <providers/live_stream_provider.h>
#include <core/resource/camera_resource.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <media_server/settings.h>

#include "mediaserver_ini.h"
#include <media_server/media_server_module.h>
#include <core/dataprovider/data_provider_factory.h>

class QnDataProviderFactory;

using nx::api::ImageRequest;

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
    QnVideoCameraGopKeeper(
        QnVideoCamera* camera, const QnResourcePtr &resource,  QnServer::ChunksCatalog catalog);

    virtual ~QnVideoCameraGopKeeper();

    virtual void beforeDisconnectFromResource();

    int copyLastGop(qint64 skipTime, QnDataPacketQueue& dstQueue, bool iFramesOnly);

    virtual bool canAcceptData() const;
    virtual void putData(const QnAbstractDataPacketPtr& data);
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    QnConstCompressedVideoDataPtr getLastVideoFrame(int channel) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame() const;

    void updateCameraActivity();
    virtual bool needConfigureProvider() const override { return false; }
    void clearVideoData();

    std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        qint64 timeUs, int channel, ImageRequest::RoundMethod roundMethod) const;

private:
    QnConstCompressedVideoDataPtr getIframeByTimeUnsafe(
        qint64 time, int channel, ImageRequest::RoundMethod roundMethod) const;

    QnConstCompressedVideoDataPtr getIframeByTime(
        qint64 time, int channel, ImageRequest::RoundMethod roundMethod) const;

    std::unique_ptr<QnConstDataPacketQueue> getGopTillTime(qint64 time, int channel) const;

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

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnVideoCamera* camera, const QnResourcePtr& resource, QnServer::ChunksCatalog catalog):
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

static CyclicAllocator gopKeeperKeyFramesAllocator;

static QnAbstractAllocator* getAllocator(size_t frameSize)
{
    const static size_t kMaxFrameSize = CyclicAllocator::DEFAULT_ARENA_SIZE / 2;
    return frameSize < kMaxFrameSize
            ? static_cast<QnAbstractAllocator*>(&gopKeeperKeyFramesAllocator)
            : static_cast<QnAbstractAllocator*>(QnSystemAllocator::instance());
}

void QnVideoCameraGopKeeper::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    if (m_allChannelsMask == 0)
    {
        auto layout = qSharedPointerDynamicCast<QnMediaResource>(m_resource)->getVideoLayout();
        m_allChannelsMask = (1 << layout->channelCount()) - 1;
    }

    QnMutexLocker lock(&m_queueMtx);
    if (QnConstCompressedVideoDataPtr video =
        std::dynamic_pointer_cast<const QnCompressedVideoData>(nonConstData))
    {
        NX_VERBOSE(this) << lm("%1(%2 us, %3-frame)")
            .args(__func__, nonConstData->timestamp, (video->flags & AV_PKT_FLAG_KEY) ? "I" : "P");

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

            if (m_lastKeyFrames[ch].empty()
                || m_lastKeyFrames[ch].back()->timestamp <=
                    video->timestamp - KEEP_IFRAMES_DISTANCE)
            {
                m_lastKeyFrames[ch].push_back(QnCompressedVideoDataPtr(
                    video->clone(getAllocator(video->dataSize()))));
            }

            while ((!m_lastKeyFrames[ch].empty()
                && m_lastKeyFrames[ch].front()->timestamp < removeThreshold)
                || m_lastKeyFrames[ch].size() > KEEP_IFRAMES_INTERVAL/KEEP_IFRAMES_DISTANCE)
            {
                m_lastKeyFrames[ch].pop_front();
            }
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize())
        {
            // TODO: #ak: MUST NOT modify video packet here! It can be used by other threads
            // concurrently and flags value can be undefined in other threads.
            static_cast<QnAbstractMediaData*>(nonConstData.get())->flags |=
                QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData(nonConstData);
        }
    }
    else if (QnConstCompressedAudioDataPtr audio =
        std::dynamic_pointer_cast<const QnCompressedAudioData>(nonConstData))
    {
        NX_VERBOSE(this) << lm("%1(%2 us, audio)").args(__func__, nonConstData->timestamp);
        m_lastAudioData = std::move(audio);
    }
    else
    {
        NX_VERBOSE(this) << lm("%1(%2 us): Ignored unsupported data")
            .args(__func__, nonConstData->timestamp);
    }
}

bool QnVideoCameraGopKeeper::processData(const QnAbstractDataPacketPtr& /*data*/)
{
    return true;
}

int QnVideoCameraGopKeeper::copyLastGop(qint64 skipTime, QnDataPacketQueue& dstQueue, bool iFramesOnly)
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
                /**
                 * data->opaque is used here to designate cseq (Command Sequence Number, see
                 * QnDataConsumer::setWaitCSeq for details).
                 * When playing live stream, several frames from the GopKeeper are pushed in the
                 * stream  before real live frames are played. Since live frames always have
                 * cseq == 0, we force GopKeeper frames to have the same cseq.
                 */
                newData->opaque = 0;
                dstQueue.push(QnAbstractMediaDataPtr(newData));
            }
            else {
                dstQueue.push(std::const_pointer_cast<QnAbstractDataPacket>(data)); // TODO: #ak remove const_cast
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
    qint64 timeUs, int channel, ImageRequest::RoundMethod roundMethod) const
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
        timeUs,
        condition);

    if (itr == queue.end())
    {
        const bool returnLastKeyFrame =
            m_lastKeyFrame[channel]
            && (m_lastKeyFrame[channel]->timestamp <= timeUs
                || roundMethod != ImageRequest::RoundMethod::iFrameBefore);

        if (returnLastKeyFrame)
            return m_lastKeyFrame[channel];
        else
            return queue.back();
    }

    const bool returnIframeBeforeTime =
        itr != queue.begin()
        && (*itr)->timestamp > timeUs
        && roundMethod == ImageRequest::RoundMethod::iFrameBefore;

    if (returnIframeBeforeTime)
        --itr; // prefer frame before defined time if no exact match

    return QnConstCompressedVideoDataPtr((*itr)->clone());
}

QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getIframeByTime(
    qint64 timeUs, int channel, ImageRequest::RoundMethod roundMethod) const
{
    QnMutexLocker lock( &m_queueMtx );
    return getIframeByTimeUnsafe(timeUs, channel, roundMethod);
}

/** @return Frames from I-frame up to the desired one. Can be empty but not null. */
std::unique_ptr<QnConstDataPacketQueue> QnVideoCameraGopKeeper::getGopTillTime(
    qint64 timeUs, int channel) const
{
    NX_VERBOSE(this) << lm("%1(%2 us, channel: %3) BEGIN").args(__func__, timeUs, channel);

    QnMutexLocker lock(&m_queueMtx);

    auto frames = std::make_unique<QnConstDataPacketQueue>();
    const auto dataQueue = m_dataQueue.lock();
    for (int i = 0; i < dataQueue.size(); ++i)
    {
        const QnConstAbstractDataPacketPtr& data = dataQueue.at(i);
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(data);
        const qint64 tUs = video->timestamp;
        if (video && tUs <= timeUs && video->channelNumber == (quint32) channel)
        {
            NX_VERBOSE(this) << lm("%1(): Adding frame %2 (%3%4) us").args(__func__,
                tUs, (tUs >= timeUs) ? "+" : "-", std::abs(tUs - timeUs));
            frames->push(video);
        }
        else
        {
            NX_VERBOSE(this) << lm("%1(): Skipping frame %2 (%3%4) us").args(__func__,
                tUs, (tUs >= timeUs) ? "+" : "-", std::abs(tUs - timeUs));
        }
    }

    if (frames->isEmpty())
        NX_VERBOSE(this) << lm("%1() END -> empty").arg(__func__);
    else
        NX_VERBOSE(this) << lm("%1() END -> %2 frame(s)").args(__func__, frames->size());
    return frames;
}

std::unique_ptr<QnConstDataPacketQueue> QnVideoCameraGopKeeper::getFrameSequenceByTime(
    qint64 timeUs, int channel, ImageRequest::RoundMethod roundMethod) const
{
    NX_VERBOSE(this) << lm("%1(%2 us, channel: %3, %4) BEGIN")
        .args(__func__, timeUs, channel, roundMethod);
    if (roundMethod == ImageRequest::RoundMethod::precise)
    {
        auto frames = getGopTillTime(timeUs, channel);
        if (frames->isEmpty())
        {
            NX_VERBOSE(this) << lm("%1() END -> null: No 'precise' in GopKeeper")
                .args(__func__);
            return nullptr;
        }

        NX_VERBOSE(this) << lm("%1() END -> %2 frame(s): Got 'precise' from GopKeeper")
            .args(__func__, frames->size());
        return frames;
    }

    auto iFrame = getIframeByTime(timeUs, channel, roundMethod);
    if (!iFrame)
    {
        NX_VERBOSE(this) << lm("%1() END -> null: No I-frame in GopKeeper").args(__func__);
        return nullptr;
    }

    if (roundMethod == ImageRequest::RoundMethod::iFrameAfter && iFrame->timestamp < timeUs)
    {
        NX_VERBOSE(this) << lm(
            "%1(): Got 'before' I-frame but requested 'after' => attempt 'precise'").arg(__func__);
        auto frames = getGopTillTime(timeUs, channel);
        if (frames->isEmpty())
        {
            frames->push(iFrame);
            NX_VERBOSE(this) << lm("%1() END -> iFrame 'before': No 'precise' in GopKeeper")
                .arg(__func__);
            return frames;
        }

        NX_VERBOSE(this) << lm("%1() END -> %2 frame(s): Got 'after' from GopKeeper")
            .args(__func__, frames->size());
        return frames;
    }

    if (roundMethod == ImageRequest::RoundMethod::iFrameBefore && iFrame->timestamp > timeUs)
    {
        NX_VERBOSE(this) << lm("%1() END -> null: Got 'after' I-frame but requested 'before'")
            .arg(__func__);
        return nullptr;
    }

    auto frames = std::make_unique<QnConstDataPacketQueue>();
    frames->push(iFrame);
    NX_VERBOSE(this) << lm("%1() END -> iFrame").arg(__func__);
    return frames;
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
        m_resource->commonModule()->globalSettings()->isAutoUpdateThumbnailsEnabled())
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

QnVideoCamera::QnVideoCamera(
    const nx::mediaserver::Settings& settings,
    QnDataProviderFactory* dataProviderFactory,
    const QnResourcePtr& resource)
    :
    m_settings(settings),
    m_dataProviderFactory(dataProviderFactory),
    m_resource(resource),
    m_primaryGopKeeper(nullptr),
    m_secondaryGopKeeper(nullptr),
    m_loStreamHlsInactivityPeriodMS(m_settings.hlsInactivityPeriod() * MSEC_PER_SEC),
    m_hiStreamHlsInactivityPeriodMS(m_settings.hlsInactivityPeriod() * MSEC_PER_SEC)
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
    QnMutexLocker lock(&m_getReaderMutex);

    const QnSecurityCamResource* cameraResource =
        dynamic_cast<QnSecurityCamResource*>(m_resource.data());
    if (cameraResource)
    {
        if (!cameraResource->hasDualStreaming() && m_secondaryReader)
        {
            if (m_secondaryReader->isRunning())
                m_secondaryReader->pleaseStop();
        }

        if (cameraResource->flags() & Qn::foreigner)
        {
            // Clearing saved key frames.
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

    if (!cameraResource->hasVideo() && !cameraResource->isAudioSupported())
        return;

    QnLiveStreamProviderPtr &reader = primaryLiveStream ? m_primaryReader : m_secondaryReader;
    if (reader == 0)
    {
        QnAbstractStreamDataProvider* dataProvider = NULL;
        if ( primaryLiveStream || (cameraResource && cameraResource->hasDualStreaming()) )
            dataProvider = m_dataProviderFactory->createDataProvider(m_resource, role);

        if ( dataProvider )
        {
            reader = QnLiveStreamProviderPtr(dynamic_cast<QnLiveStreamProvider*>(dataProvider));
            if (reader == 0)
            {
                delete dataProvider;
            } else
            {
                reader->setOwner(toSharedPointer());
                // TODO: make at_camera_resourceChanged async (queued connection e.t.c)
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

    const QnSecurityCamResource* cameraResource =
        dynamic_cast<QnSecurityCamResource*>(m_resource.data());

    if (m_secondaryReader)
    {
        if (!m_liveCache[MEDIA_Quality_Low])
        {
            ensureLiveCacheStarted(
                MEDIA_Quality_Low,
                m_secondaryReader,
                duration_cast<microseconds>(m_settings.hlsTargetDurationMS()).count());
        }
    }

    if (!m_liveCache[MEDIA_Quality_High] && m_primaryReader
        && (ini().forceLiveCacheForPrimaryStream || !m_secondaryReader))
    {
        // If the camera has one stream only and it is suitable for motion, then cache the primary
        // stream.
        bool needToCachePrimaryStream =
            !cameraResource->hasDualStreaming()
            && cameraResource->hasCameraCapabilities(Qn::PrimaryStreamSoftMotionCapability);

        if (ini().forceLiveCacheForPrimaryStream || needToCachePrimaryStream)
        {
            NX_VERBOSE(this) << lm("ATTENTION: Enabling liveCache for the primary stream");
            ensureLiveCacheStarted(
                MEDIA_Quality_High,
                m_primaryReader,
                duration_cast<microseconds>(m_settings.hlsTargetDurationMS()).count());
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

QnLiveStreamProviderPtr QnVideoCamera::getLiveReader(QnServer::ChunksCatalog catalog, bool ensureInitialized)
{
    QnMutexLocker lock( &m_getReaderMutex );
    return getLiveReaderNonSafe( catalog, ensureInitialized);
}

int QnVideoCamera::copyLastGop(
    bool primaryLiveStream,
    qint64 skipTime,
    QnDataPacketQueue& dstQueue,
    bool iFramesOnly)
{
    if (primaryLiveStream && m_primaryGopKeeper)
        return m_primaryGopKeeper->copyLastGop(skipTime, dstQueue, iFramesOnly);
    else if (m_secondaryGopKeeper)
        return m_secondaryGopKeeper->copyLastGop(skipTime, dstQueue, iFramesOnly);
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
    bool primaryLiveStream, qint64 time, int channel, ImageRequest::RoundMethod roundMethod) const
{
    NX_VERBOSE(this) << lm("%1(%2, %3 us, channel: %4, %5)")
        .args(__func__, primaryLiveStream ? "primary" : "secondary", time, channel, roundMethod);

    QnVideoCameraGopKeeper* gopKeeper =
        primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;

    if (!gopKeeper)
        return nullptr;

    return gopKeeper->getFrameSequenceByTime(time, channel, roundMethod);
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
    {
        QnMutexLocker lock(&m_getReaderMutex);
        m_cameraUsers << user;
        m_lastActivityTimer.restart();
    }

    // This call is required so camera is ready to use right after inUse call.
    updateActivity();
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
                !cameraResource->hasDualStreaming() &&
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

nx::mediaserver::hls::LivePlaylistManagerPtr QnVideoCamera::hlsLivePlaylistManager( MediaQuality streamQuality ) const
{
    return streamQuality < m_hlsLivePlaylistManager.size()
        ? m_hlsLivePlaylistManager[streamQuality]
        : nx::mediaserver::hls::LivePlaylistManagerPtr();
}

QnResourcePtr QnVideoCamera::resource() const
{
    return m_resource;
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
            getLiveReaderNonSafe( QnServer::HiQualityCatalog, true);
        if( !m_primaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_primaryReader, targetDurationUSec );
    }

    if( streamQuality == MEDIA_Quality_Low )
    {
        if( !m_secondaryReader )
            getLiveReaderNonSafe( QnServer::LowQualityCatalog, true);
        if( !m_secondaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_secondaryReader, targetDurationUSec );
    }

    return false;
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReaderNonSafe(QnServer::ChunksCatalog catalog, bool ensureInitialized)
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
    else if (ensureInitialized)
    {
        m_resource->initAsync( true );
    }

    const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
    if ( cameraResource && !cameraResource->hasDualStreaming() && catalog == QnServer::LowQualityCatalog )
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

        int removedChunksToKeepCount = m_settings.hlsRemovedChunksToKeep();
        m_hlsLivePlaylistManager[streamQuality] =
            std::make_shared<nx::mediaserver::hls::LivePlaylistManager>(
                m_liveCache[streamQuality].get(),
                targetDurationUSec,
                removedChunksToKeepCount);
    }
    //connecting live cache to reader
    primaryReader->addDataProcessor( m_liveCache[streamQuality].get() );
    return true;
}
