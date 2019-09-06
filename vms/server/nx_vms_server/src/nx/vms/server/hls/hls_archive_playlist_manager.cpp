#include "hls_archive_playlist_manager.h"

#include <limits>

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/thread/mutex.h>

#include <core/resource/security_cam_resource.h>
#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <media_server/media_server_module.h>

#include "streaming/streaming_chunk_cache_key.h"

namespace nx::vms::server::hls {

/** Such a huge GOP is possible in case of low fps stream. */
static constexpr qint64 kMaxGopLengthSec = 100;
static constexpr qint64 kUsecPerMs = 1000;
static constexpr qint64 kMillisPerSec = 1000;
static constexpr qint64 kDefaultNextIFrameLoopSize = 5;

ArchivePlaylistManager::ArchivePlaylistManager(
    QnMediaServerModule* serverModule,
    const QnSecurityCamResourcePtr& camResource,
    const QnUuid& clientUuid,
    qint64 startTimestamp,
    unsigned int maxChunkNumberInPlaylist,
    std::chrono::microseconds targetDuration,
    MediaQuality streamQuality)
    :
    ServerModuleAware(serverModule),
    m_camResource(camResource),
    m_clientUuid(clientUuid),
    m_startTimestamp(startTimestamp),
    m_maxChunkNumberInPlaylist(maxChunkNumberInPlaylist),
    m_targetDuration(targetDuration),
    m_streamQuality(streamQuality),
    m_totalPlaylistDuration(0),
    m_prevChunkEndTimestamp(0),
    m_eof(false),
    m_chunkMediaSequence(0),
    m_timerCorrection(0),
    m_initialPlaylistCreated(false),
    m_prevGeneratedChunkDuration(0),
    m_discontinuityDetected(false)
{
    NX_ASSERT(m_maxChunkNumberInPlaylist > 0);
    m_getNextIFrameLoopMaxSize = std::max<qint64>(
        (kMaxGopLengthSec * kUsecPerMs * kMillisPerSec) / m_targetDuration.count(),
        kDefaultNextIFrameLoopSize);
}

ArchivePlaylistManager::~ArchivePlaylistManager()
{
}

CameraDiagnostics::Result ArchivePlaylistManager::createDelegate(QnAbstractArchiveDelegatePtr& result)
{
    QnAbstractArchiveDelegatePtr archiveDelegate(m_camResource->createArchiveDelegate());
    if (archiveDelegate)
        archiveDelegate->setGroupId(m_clientUuid.toByteArray());

    if (!archiveDelegate)
    {
        archiveDelegate = QnAbstractArchiveDelegatePtr(new QnServerArchiveDelegate(serverModule())); // default value
        if (!archiveDelegate->open(m_camResource, serverModule()->archiveIntegrityWatcher()))
            return archiveDelegate->lastError();
    }
    if (!archiveDelegate->setQuality(
        m_streamQuality == MEDIA_Quality_High ? MEDIA_Quality_ForceHigh : m_streamQuality,
        true,
        QSize()))
    {
        return archiveDelegate->lastError();
    }
    archiveDelegate->setPlaybackMode(PlaybackMode::ThumbNails);

    if (archiveDelegate->getFlags().testFlag(QnAbstractArchiveDelegate::Flag_CanProcessMediaStep))
        result = archiveDelegate;
    else
        result.reset(new QnThumbnailsArchiveDelegate(archiveDelegate));

    result->setRange(
        m_startTimestamp,
        std::numeric_limits<qint64>::max(),
        m_targetDuration.count());

    if (m_prevChunkEndTimestamp <= 0)
    {
        const QnAbstractMediaDataPtr& nextData = result->getNextData();
        if (!nextData)
        {
            m_prevChunkEndTimestamp = -1;
            return archiveDelegate->lastError();
        }

        m_prevChunkEndTimestamp = nextData->timestamp;
        m_currentArchiveChunk = result->getLastUsedChunkInfo();

        m_initialPlaylistCreated = false;
    }
    return archiveDelegate->lastError();
}

CameraDiagnostics::Result ArchivePlaylistManager::generateChunkList(
    std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
    bool* const endOfStreamReached) const
{
    QnMutexLocker lk(&m_mutex);

    auto status = const_cast<ArchivePlaylistManager*>(this)->generateChunksIfNeeded();

    std::copy(m_chunks.cbegin(), m_chunks.cend(), std::back_inserter(*chunkList));
    if (endOfStreamReached)
        *endOfStreamReached = m_eof;
    return status;
}

int ArchivePlaylistManager::getMaxBitrate() const
{
    //TODO/HLS: #ak
    return -1;
}

CameraDiagnostics::Result ArchivePlaylistManager::generateChunksIfNeeded()
{
    if (!m_initialPlaylistCreated)
    {
        //creating initial playlist
        for (size_t i = 0; i < m_maxChunkNumberInPlaylist && !m_eof; ++i)
        {
            auto status = addOneMoreChunk();
            if (!status)
                return status;
        }
        m_initialPlaylistCreated = true;
        m_playlistUpdateTimer.start();
        m_timerCorrection = 0;
    }
    else if (!m_eof)
    {
        if ((m_playlistUpdateTimer.elapsed() * kUsecPerMs - m_timerCorrection) > m_prevGeneratedChunkDuration)
        {
            auto status = addOneMoreChunk();
            if (!status)
                return status;
            m_timerCorrection += m_prevGeneratedChunkDuration;
        }
    }
    return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::noError);
}

CameraDiagnostics::Result ArchivePlaylistManager::addOneMoreChunk()
{
    if (!m_delegate)
    {
        auto status = createDelegate(m_delegate);
        if (!status || !m_delegate)
        {
            NX_ERROR(this, "Failed to create archive delegate, error code: %1", status.errorCode);
            m_delegate.reset();
            m_prevChunkEndTimestamp = 0;
            return status;
        }
    }

    QnAbstractMediaDataPtr nextData;
    for (int i = 0; i < m_getNextIFrameLoopMaxSize; ++i)
    {
        nextData = m_delegate->getNextData();
        if (!nextData)
        {
            //end of archive reached
            //TODO/HLS: #ak end of archive is moving forward constantly, so need just imply some delay
            m_eof = true;
            return m_delegate->lastError();
        }
        if (nextData->timestamp != m_prevChunkEndTimestamp)
            break;
    }
    NX_ASSERT(nextData->timestamp != m_prevChunkEndTimestamp);
    if (nextData->timestamp == m_prevChunkEndTimestamp)
        return m_delegate->lastError();

    const qint64 currentChunkEndTimestamp = nextData->timestamp;

    AbstractPlaylistManager::ChunkData chunkData;
    chunkData.mediaSequence = ++m_chunkMediaSequence;
    chunkData.startTimestamp = m_prevChunkEndTimestamp;
    chunkData.discontinuity = m_discontinuityDetected;
    if (nextData->flags & QnAbstractMediaData::MediaFlags_BOF)
    {
        //gap in archive detected
            if (m_prevChunkEndTimestamp >= m_currentArchiveChunk.startTimeUsec &&
                m_prevChunkEndTimestamp < (m_currentArchiveChunk.startTimeUsec + m_currentArchiveChunk.durationUsec))
        {
            chunkData.duration = m_currentArchiveChunk.durationUsec - (m_prevChunkEndTimestamp - m_currentArchiveChunk.startTimeUsec);
            NX_ASSERT(chunkData.duration > 0);
        }
        else
        {
            //TODO/HLS: #ak some correction is required to call addOneMoreChunk() at appropriate time
            chunkData.duration = m_targetDuration.count();
        }
        //have to insert EXT-X-DISCONTINUITY tag before next chunk
        m_discontinuityDetected = true;
    }
    else
    {
        chunkData.duration = currentChunkEndTimestamp - chunkData.startTimestamp;
        m_discontinuityDetected = false;
    }
    m_totalPlaylistDuration += chunkData.duration;
    m_chunks.push_back(chunkData);

    if (m_chunks.size() > m_maxChunkNumberInPlaylist)
    {
        m_totalPlaylistDuration -= m_chunks.front().duration;
        m_chunks.pop_front();
    }

    m_prevChunkEndTimestamp = currentChunkEndTimestamp;
    m_prevGeneratedChunkDuration = chunkData.duration;
    m_currentArchiveChunk = m_delegate->getLastUsedChunkInfo();

    auto thumbDelegate = m_delegate.dynamicCast<QnThumbnailsArchiveDelegate>();
    if (thumbDelegate)
    {
        m_startTimestamp = thumbDelegate->currentPosition();
        m_delegate.reset();
    }

    return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::noError);
}

} // namespace nx::vms::server::hls
