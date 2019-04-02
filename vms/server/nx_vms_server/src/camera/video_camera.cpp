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
#include <nx/vms/server/resource/camera.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <media_server/settings.h>

#include <nx_vms_server_ini.h>
#include <media_server/media_server_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <camera/gop_keeper.h>

class QnDataProviderFactory;

using nx::api::ImageRequest;

static const qint64 MSEC_PER_SEC = 1000;
static const qint64 USEC_PER_MSEC = 1000;
static unsigned int MEDIA_CACHE_SIZE_MILLIS = 10 * MSEC_PER_SEC;
static const int CAMERA_PULLING_STOP_TIMEOUT = 1000 * 3;

namespace nx::vms::server {

VideoCamera::VideoCamera(
    const nx::vms::server::Settings& settings,
    QnDataProviderFactory* dataProviderFactory,
    const nx::vms::server::resource::CameraPtr& camera)
    :
    m_settings(settings),
    m_dataProviderFactory(dataProviderFactory),
    m_camera(camera),
    m_primaryGopKeeper(nullptr),
    m_secondaryGopKeeper(nullptr),
    m_loStreamHlsInactivityPeriodMS(m_settings.hlsInactivityPeriod() * MSEC_PER_SEC),
    m_hiStreamHlsInactivityPeriodMS(m_settings.hlsInactivityPeriod() * MSEC_PER_SEC)
{
    // Ensuring that vectors will not take much memory.
    static_assert(
        ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
        "MediaQuality enum suddenly contains too large values: consider changing VideoCamera::m_liveCache, VideoCamera::m_hlsLivePlaylistManager type" );

    m_liveCache.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_hlsLivePlaylistManager.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_lastActivityTimer.invalidate();
}

nx::vms::server::GopKeeper* VideoCamera::getGopKeeper(StreamIndex streamIndex) const
{
    switch (streamIndex)
    {
        case StreamIndex::primary:
            return m_primaryGopKeeper;
        case StreamIndex::secondary:
            return m_secondaryGopKeeper;
        default:
            break;
    }
    NX_ASSERT(false, lm("Unsupported stream index %1").arg((int) streamIndex));
    return nullptr; //< Fallback for the failed assertion.
}

void VideoCamera::beforeStop()
{
    QnMutexLocker lock(&m_getReaderMutex);

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
        m_primaryReader->removeDataProcessor(m_liveCache[MEDIA_Quality_High].get());
        m_primaryReader->pleaseStop();
    }

    if (m_secondaryReader) {
        m_secondaryReader->removeDataProcessor(m_secondaryGopKeeper);
        m_secondaryReader->removeDataProcessor(m_liveCache[MEDIA_Quality_Low].get());
        m_secondaryReader->pleaseStop();
    }
}

void VideoCamera::stop()
{
    if (m_primaryReader)
        m_primaryReader->stop();
    if (m_secondaryReader)
        m_secondaryReader->stop();
}

VideoCamera::~VideoCamera()
{
    beforeStop();
    stop();
    delete m_primaryGopKeeper;
    delete m_secondaryGopKeeper;
}

void VideoCamera::at_camera_resourceChanged()
{
    QnMutexLocker lock(&m_getReaderMutex);

    if (!m_camera->hasDualStreaming() && m_secondaryReader)
    {
        if (m_secondaryReader->isRunning())
            m_secondaryReader->pleaseStop();
    }

    if (m_camera->flags() & Qn::foreigner)
    {
        // Clearing saved key frames.
        if (m_primaryGopKeeper)
            m_primaryGopKeeper->clearVideoData();
        if (m_secondaryGopKeeper)
            m_secondaryGopKeeper->clearVideoData();
    }
}

void VideoCamera::createReader(QnServer::ChunksCatalog catalog)
{
    const StreamIndex streamIndex = (catalog == QnServer::HiQualityCatalog)
        ? StreamIndex::primary
        : StreamIndex::secondary;
    const Qn::ConnectionRole role = QnSecurityCamResource::toConnectionRole(streamIndex);

    if (!m_camera->hasVideo() && !m_camera->isAudioSupported())
        return;

    QnLiveStreamProviderPtr &reader =
        (streamIndex == StreamIndex::primary) ? m_primaryReader : m_secondaryReader;
    if (!reader)
    {
        QnAbstractStreamDataProvider* dataProvider = nullptr;
        if (streamIndex == StreamIndex::primary || m_camera->hasDualStreaming())
        {
            dataProvider = m_dataProviderFactory->createDataProvider(m_camera, role);
        }

        if (dataProvider)
        {
            reader = QnLiveStreamProviderPtr(dynamic_cast<QnLiveStreamProvider*>(dataProvider));
            if (!reader)
            {
                delete dataProvider;
            }
            else
            {
                reader->setOwner(toSharedPointer());
                // TODO: Make at_camera_resourceChanged async (queued connection, etc.).
                if (role == Qn::CR_LiveVideo)
                {
                    QObject::connect(reader->getResource().data(), &QnResource::resourceChanged,
                        this, &VideoCamera::at_camera_resourceChanged, Qt::DirectConnection);
                }

                nx::vms::server::GopKeeper* gopKeeper =
                    new nx::vms::server::GopKeeper(this, m_camera, catalog);
                if (streamIndex == StreamIndex::primary)
                    m_primaryGopKeeper = gopKeeper;
                else
                    m_secondaryGopKeeper = gopKeeper;
                reader->addDataProcessor(gopKeeper);
            }
        }
    }
}

void VideoCamera::startLiveCacheIfNeeded()
{
    using namespace std::chrono;

    QnMutexLocker lock(&m_getReaderMutex);

    if (!isSomeActivity())
        return;

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
            !m_camera->hasDualStreaming()
            && m_camera->hasCameraCapabilities(Qn::PrimaryStreamSoftMotionCapability);

        if (ini().forceLiveCacheForPrimaryStream || needToCachePrimaryStream)
        {
            NX_VERBOSE(this, "ATTENTION: Enabling liveCache for the primary stream");
            ensureLiveCacheStarted(
                MEDIA_Quality_High,
                m_primaryReader,
                duration_cast<microseconds>(m_settings.hlsTargetDurationMS()).count());
        }
    }
}

QnLiveStreamProviderPtr VideoCamera::getLiveReader(
    QnServer::ChunksCatalog catalog, bool ensureInitialized, bool createIfNotExist)
{
    QnMutexLocker lock( &m_getReaderMutex );
    if (!createIfNotExist)
        return catalog == QnServer::HiQualityCatalog ? m_primaryReader : m_secondaryReader;

    return getLiveReaderNonSafe( catalog, ensureInitialized);
}

int VideoCamera::copyLastGop(
    StreamIndex streamIndex,
    qint64 skipTime,
    QnDataPacketQueue& dstQueue,
    bool iFramesOnly)
{
    if (streamIndex == StreamIndex::primary && m_primaryGopKeeper)
        return m_primaryGopKeeper->copyLastGop(skipTime, dstQueue, iFramesOnly);
    if (m_secondaryGopKeeper)
        return m_secondaryGopKeeper->copyLastGop(skipTime, dstQueue, iFramesOnly);
    return 0;
}

std::unique_ptr<QnConstDataPacketQueue> VideoCamera::getFrameSequenceByTime(
    StreamIndex streamIndex,
    qint64 time,
    int channel,
    ImageRequest::RoundMethod roundMethod) const
{
    NX_VERBOSE(this, "%1(%2, %3 us, channel: %4, %5)", __func__,
        streamIndex, time, channel, roundMethod);

    if (auto gopKeeper = getGopKeeper(streamIndex))
        return gopKeeper->getFrameSequenceByTime(time, channel, roundMethod);
    return nullptr;
}

QnConstCompressedVideoDataPtr VideoCamera::getLastVideoFrame(
    StreamIndex streamIndex, int channel) const
{
    if (auto gopKeeper = getGopKeeper(streamIndex))
        return gopKeeper->getLastVideoFrame(channel);
    return nullptr;
}

QnConstCompressedAudioDataPtr VideoCamera::getLastAudioFrame(StreamIndex streamIndex) const
{
    if (auto gopKeeper = getGopKeeper(streamIndex))
        return gopKeeper->getLastAudioFrame();
    return nullptr;
}

void VideoCamera::inUse(void* user)
{
    {
        QnMutexLocker lock(&m_getReaderMutex);
        m_cameraUsers << user;
        m_lastActivityTimer.restart();
    }

    // This call is required so camera is ready to use right after inUse call.
    updateActivity();
}

void VideoCamera::notInUse(void* user)
{
    QnMutexLocker lock( &m_getReaderMutex );
    m_cameraUsers.remove(user);
    m_lastActivityTimer.restart();
}

bool VideoCamera::isSomeActivity() const
{
    return !m_cameraUsers.isEmpty() && !m_camera->hasFlags(Qn::foreigner);
}

void VideoCamera::updateActivity()
{
    //this method is called periodically

    if (m_primaryGopKeeper)
        m_primaryGopKeeper->updateCameraActivity();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->updateCameraActivity();
    stopIfNoActivity();
    startLiveCacheIfNeeded();
}

void VideoCamera::stopIfNoActivity()
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
            // If single stream is available and that stream is suitable for motion then stopping
            // caching only if nothing else fetches stream.
            const bool isSingleStreamCameraAndHiSuitableForCaching =
                !m_camera->hasDualStreaming() &&
                m_camera->hasCameraCapabilities( Qn::PrimaryStreamSoftMotionCapability );
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

const MediaStreamCache* VideoCamera::liveCache( MediaQuality streamQuality ) const
{
    return streamQuality < m_liveCache.size()
        ? m_liveCache[streamQuality].get()
        : nullptr;
}

MediaStreamCache* VideoCamera::liveCache( MediaQuality streamQuality )
{
    return streamQuality < m_liveCache.size()
        ? m_liveCache[streamQuality].get()
        : nullptr;
}

nx::vms::server::hls::LivePlaylistManagerPtr VideoCamera::hlsLivePlaylistManager( MediaQuality streamQuality ) const
{
    return streamQuality < m_hlsLivePlaylistManager.size()
        ? m_hlsLivePlaylistManager[streamQuality]
        : nx::vms::server::hls::LivePlaylistManagerPtr();
}

QnResourcePtr VideoCamera::resource() const
{
    return m_camera;
}

/**
 * Starts caching live stream, if not started.
 * @return True if started, false if failed to start.
 */
bool VideoCamera::ensureLiveCacheStarted(MediaQuality streamQuality, qint64 targetDurationUSec)
{
    QnMutexLocker lock(&m_getReaderMutex);

    NX_ASSERT(streamQuality == MEDIA_Quality_High || streamQuality == MEDIA_Quality_Low);

    if (streamQuality == MEDIA_Quality_High)
    {
        if (!m_primaryReader)
            getLiveReaderNonSafe(QnServer::HiQualityCatalog, true);
        if (!m_primaryReader)
            return false;
        return ensureLiveCacheStarted(streamQuality, m_primaryReader, targetDurationUSec);
    }

    if (streamQuality == MEDIA_Quality_Low)
    {
        if (!m_secondaryReader)
            getLiveReaderNonSafe(QnServer::LowQualityCatalog, true);
        if (!m_secondaryReader)
            return false;
        return ensureLiveCacheStarted(streamQuality, m_secondaryReader, targetDurationUSec);
    }

    return false;
}

QnLiveStreamProviderPtr VideoCamera::getLiveReaderNonSafe(
    QnServer::ChunksCatalog catalog, bool ensureInitialized)
{
    if (m_camera->hasFlags(Qn::foreigner))
        return nullptr;

    if (m_camera->isInitialized())
    {
        if ((catalog == QnServer::HiQualityCatalog && m_primaryReader == 0)
            || (catalog == QnServer::LowQualityCatalog && m_secondaryReader == 0))
        {
            createReader(catalog);
        }
    }
    else if (ensureInitialized)
    {
        NX_VERBOSE(this, "Trying to init not initialized camera [%1]", m_camera);
        m_camera->initAsync(true);
    }

    if (m_camera && !m_camera->hasDualStreaming()
        && catalog == QnServer::LowQualityCatalog)
    {
        return nullptr;
    }

    return catalog == QnServer::HiQualityCatalog ? m_primaryReader : m_secondaryReader;
}

/**
 * Starts caching live stream, if not started.
 * @return True if started, false if failed to start.
 */
bool VideoCamera::ensureLiveCacheStarted(
    MediaQuality streamQuality,
    const QnLiveStreamProviderPtr& primaryReader,
    qint64 targetDurationUSec )
{
    primaryReader->startIfNotRunning();

    m_cameraUsers.insert(this);

    if (!m_liveCache[streamQuality].get())
    {
        m_liveCache[streamQuality].reset(new MediaStreamCache(
            MEDIA_CACHE_SIZE_MILLIS,
            // Hls spec requires 7 last chunks to be in memory, adding extra 3 just in case.
            MEDIA_CACHE_SIZE_MILLIS * 10));

        int removedChunksToKeepCount = m_settings.hlsRemovedChunksToKeep();
        m_hlsLivePlaylistManager[streamQuality] =
            std::make_shared<nx::vms::server::hls::LivePlaylistManager>(
                m_liveCache[streamQuality].get(),
                targetDurationUSec,
                removedChunksToKeepCount);
    }

    // Connecting live cache to the reader.
    primaryReader->addDataProcessor(m_liveCache[streamQuality].get());
    return true;
}

} // namespace nx::vms::server
