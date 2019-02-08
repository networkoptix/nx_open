#pragma once

#include <deque>
#include <memory>
#include <queue>

#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include "hls_playlist_manager.h"
#include "utils/media/media_stream_cache.h"

class MediaStreamCache;

namespace nx::vms::server::hls {

/**
 * Provides live playlist for resource.
 * Updates playlist in real-time (by registering itself as an MediaStreamCache event receiver).
 * Blocks must-be-available chunks data from removal from MediaStreamCache.
*/
class LivePlaylistManager:
    public AbstractPlaylistManager
{
public:
    LivePlaylistManager(
        MediaStreamCache* const mediaStreamCache,
        quint64 targetDurationUSec,
        int removedChunksToKeepCount);
    virtual ~LivePlaylistManager();

    /**
     * Provides playlist for live data.
     * @return Number of chunks generated.
     */
    virtual size_t generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached) const override;
    virtual int getMaxBitrate() const override;

    void clear();
    /** Time (millis) from last usage of this object. */
    qint64 inactivityPeriod() const;

private:
    MediaStreamCache* const m_mediaStreamCache;
    mutable std::deque<AbstractPlaylistManager::ChunkData> m_chunks;
    const quint64 m_targetDurationUSec;
    mutable unsigned int m_mediaSequence = 0;
    AbstractPlaylistManager::ChunkData m_currentChunk;
    mutable QnMutex m_mutex;
    quint64 m_totalPlaylistDuration = 0;
    //queue<pair<timestamp to block; playlist start timestamp, blocking lives to> >
    std::queue<std::pair<quint64, quint64> > m_timestampToBlock;
    int m_blockId = -1;
    const int m_removedChunksToKeepCount;
    mutable QElapsedTimer m_inactivityTimer;
    nx::utils::SubscriptionId m_onKeyFrameSubscriptionId;
    nx::utils::SubscriptionId m_onDiscontinueSubscriptionId;

    void onKeyFrame(quint64 currentPacketTimestampUSec);
    void onDiscontinue();
};

/** Using std::shared_ptr because of std::shared_ptr::unique(). */
using LivePlaylistManagerPtr = std::shared_ptr<nx::vms::server::hls::LivePlaylistManager>;

} // namespace nx::vms::server::hls
