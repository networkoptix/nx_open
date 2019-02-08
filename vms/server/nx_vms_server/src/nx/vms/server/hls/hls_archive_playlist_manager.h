#pragma once

#include "hls_playlist_manager.h"

#include <deque>
#include <memory>

#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/avi/thumbnails_archive_delegate.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::hls {

/**
 * Generates sliding chunk list for archive playback.
 * From hls point view this class generates "LIVE" playlist
 * NOTE: It is necessary to make archive playlist sliding since archive can be very long
 *   and playlist would be quite large (~ 25Mb for a month archive).
 */
class ArchivePlaylistManager:
    public AbstractPlaylistManager,
    public ServerModuleAware
{
public:
    ArchivePlaylistManager(
        QnMediaServerModule* serverModule,
        const QnSecurityCamResourcePtr& camResource,
        qint64 startTimestamp,
        unsigned int maxChunkNumberInPlaylist,
        std::chrono::microseconds targetDuration,
        MediaQuality streamQuality);
    virtual ~ArchivePlaylistManager();

    bool initialize();

    virtual size_t generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached) const override;

    virtual int getMaxBitrate() const override;

    CameraDiagnostics::Result lastError() const;

private:
    const QnSecurityCamResourcePtr m_camResource;
    qint64 m_startTimestamp;
    unsigned int m_maxChunkNumberInPlaylist;
    std::chrono::microseconds m_targetDuration;
    qint64 m_getNextIFrameLoopMaxSize;
    MediaQuality m_streamQuality;
    mutable QnMutex m_mutex;
    std::deque<AbstractPlaylistManager::ChunkData> m_chunks;
    qint64 m_totalPlaylistDuration;
    qint64 m_prevChunkEndTimestamp;
    bool m_eof;
    int m_chunkMediaSequence;
    QnAbstractArchiveDelegatePtr m_delegate;
    QElapsedTimer m_playlistUpdateTimer;
    qint64 m_timerCorrection;
    bool m_initialPlaylistCreated;
    qint64 m_prevGeneratedChunkDuration;
    bool m_discontinuityDetected;
    /** Archive chunk, that holds last found position. */
    QnAbstractArchiveDelegate::ArchiveChunkInfo m_currentArchiveChunk;

    void generateChunksIfNeeded();
    bool addOneMoreChunk();
    qint64 endTimestamp() const;
};

/** Using std::shared_ptr for std::shared_ptr::unique(). */
using ArchivePlaylistManagerPtr = std::shared_ptr<ArchivePlaylistManager>;

} // namespace nx::vms::server::hls
