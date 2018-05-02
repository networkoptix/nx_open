#include "hls_live_playlist_manager.h"

#include <nx/utils/thread/mutex.h>

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <utils/media/media_stream_cache.h>

namespace nx {
namespace mediaserver {
namespace hls {

static const size_t CHUNKS_IN_PLAYLIST = 3;

LivePlaylistManager::LivePlaylistManager(
    MediaStreamCache* const mediaStreamCache,
    quint64 targetDurationUSec,
    int removedChunksToKeepCount)
:
    m_mediaStreamCache( mediaStreamCache ),
    m_targetDurationUSec( targetDurationUSec ),
    m_mediaSequence( 0 ),
    m_totalPlaylistDuration( 0 ),
    m_blockID( -1 ),
    m_removedChunksToKeepCount(removedChunksToKeepCount),
    m_onKeyFrameSubscriptionId(nx::utils::kInvalidSubscriptionId),
    m_onDiscontinueSubscriptionId(nx::utils::kInvalidSubscriptionId)
{
    using namespace std::placeholders;

    m_mediaStreamCache->keyFrameFoundSubscription().subscribe(
        std::bind(&LivePlaylistManager::onKeyFrame, this, _1),
        &m_onKeyFrameSubscriptionId);
    m_mediaStreamCache->streamTimeDiscontinuityFoundSubscription().subscribe(
        std::bind(&LivePlaylistManager::onDiscontinue, this),
        &m_onDiscontinueSubscriptionId);

    m_inactivityTimer.restart();
}

LivePlaylistManager::~LivePlaylistManager()
{
    if( m_blockID != -1 )
    {
        m_mediaStreamCache->unblockData( m_blockID );
        m_blockID = -1;
    }

    m_mediaStreamCache->streamTimeDiscontinuityFoundSubscription().removeSubscription(m_onDiscontinueSubscriptionId);
    m_mediaStreamCache->keyFrameFoundSubscription().removeSubscription(m_onKeyFrameSubscriptionId);
}

//!Same as \a generateChunkList, but returns \a chunksToGenerate last chunks of available data
/*!
    \return Number of chunks generated
*/
size_t LivePlaylistManager::generateChunkList(
    std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
    bool* const endOfStreamReached ) const
{
    QnMutexLocker lk( &m_mutex );

    m_inactivityTimer.restart();

    //NOTE commented code is a trick to minimize live delay with hls playback. But current implementation results in
        //playback freezing and switching to lo-quality, since downloading is done with same speed as playback and HLS client thinks bandwidth is not sufficient.
        //But, something can still be done to minimize that delay
    //if( m_chunks.empty() )
    //{
    //    //initializing with dummy chunks
    //    qint64 currentTimestamp = qnSyncTime->currentUSecsSinceEpoch();
    //    for( unsigned int i = 0; i < CHUNKS_IN_PLAYLIST; ++i )
    //    {
    //        ChunkData chunk;
    //        chunk.alias = lit("live_chunk_%1").arg(i);
    //        chunk.mediaSequence = m_currentChunk.mediaSequence + i;
    //        chunk.startTimestamp = currentTimestamp + i*m_targetDurationUSec;
    //        chunk.duration = m_targetDurationUSec;
    //        m_chunks.push_back( chunk );
    //    }
    //}

    std::copy( m_chunks.begin(), m_chunks.end(), std::back_inserter(*chunkList) );
    if( endOfStreamReached )
        *endOfStreamReached = false;
    return m_chunks.size();
}

int LivePlaylistManager::getMaxBitrate() const
{
    return m_mediaStreamCache->getMaxBitrate();
}

void LivePlaylistManager::clear()
{
    QnMutexLocker lk( &m_mutex );
    m_totalPlaylistDuration = 0;
    if( m_blockID != -1 )
    {
        m_mediaStreamCache->unblockData( m_blockID );
        m_blockID = -1;
    }
    m_chunks.clear();
    while( !m_timestampToBlock.empty() )
        m_timestampToBlock.pop();
    m_currentChunk = AbstractPlaylistManager::ChunkData();
}

qint64 LivePlaylistManager::inactivityPeriod() const
{
    QnMutexLocker lk( &m_mutex );
    return m_inactivityTimer.elapsed();
}

void LivePlaylistManager::onDiscontinue()
{
    clear();
}

void LivePlaylistManager::onKeyFrame( quint64 currentPacketTimestampUSec )
{
    QnMutexLocker lk( &m_mutex );

    NX_LOG(lit("LivePlaylistManager. got key frame %1").arg(currentPacketTimestampUSec), cl_logDEBUG2);

    if( m_currentChunk.mediaSequence > 0 )
    {
        m_currentChunk.duration = currentPacketTimestampUSec - m_currentChunk.startTimestamp;
        if( m_currentChunk.duration >= m_targetDurationUSec )
        {
            const quint64 playlistDurationBak = m_totalPlaylistDuration;

            //if there is already chunk with same media sequence, replacing it...
            std::deque<AbstractPlaylistManager::ChunkData>::iterator chunkIter = m_chunks.begin();
            for( ; chunkIter != m_chunks.end(); ++chunkIter )
                if( chunkIter->mediaSequence == m_currentChunk.mediaSequence )
                    break;
            if( chunkIter == m_chunks.end() )
            {
                m_chunks.push_back( m_currentChunk );
                NX_LOG(lit("LivePlaylistManager. Added chunk (%1:%2). Total chunks %3").
                    arg(m_chunks.back().startTimestamp).arg(m_chunks.back().duration).arg(m_chunks.size()), cl_logDEBUG2);
            }
            else
            {
                m_currentChunk.alias = chunkIter->alias;
                *chunkIter = m_currentChunk;
            }

            m_totalPlaylistDuration += m_currentChunk.duration;
            m_currentChunk.mediaSequence = 0;   //using media sequence 0 for NULL chunk
            m_currentChunk.alias.reset();

            //checking whether some chunks need to be removed
                //The server MUST NOT remove a media segment from the Playlist file if
                //   the duration of the Playlist file minus the duration of the segment
                //   is less than three times the target duration.
            while( (m_totalPlaylistDuration - m_chunks.front().duration > m_targetDurationUSec * CHUNKS_IN_PLAYLIST) &&
                   (m_chunks.size() > CHUNKS_IN_PLAYLIST) ) //in case of a large GOP (it is common for second stream)
                                                            //first condition may produce 1 or 2 chunks per playlist which is bad for iOS
            {
                //When the server removes a media segment from the Playlist, the
                //   corresponding media URI SHOULD remain available to clients for a
                //   period of time equal to the duration of the segment plus the duration
                //   of the longest Playlist file distributed by the server containing
                //   that segment.
                quint64 keepChunkDataTillTimestamp = 0;
                if( m_removedChunksToKeepCount < 0 )
                {
                    //spec-defined behavour.
                    keepChunkDataTillTimestamp = m_chunks.front().startTimestamp + m_chunks.front().duration*2 + playlistDurationBak;
                    //adding additional m_chunks.front().duration, since (m_chunks.front().startTimestamp + m_chunks.front().duration) is current playlist start timestamp
                }
                else
                {
                    if( (size_t)m_removedChunksToKeepCount < m_chunks.size() )
                        keepChunkDataTillTimestamp = m_chunks[m_removedChunksToKeepCount].startTimestamp;
                    else
                        keepChunkDataTillTimestamp =
                            m_chunks.back().startTimestamp + m_chunks.back().duration +                 //length of current playlist in usec
                            m_chunks.front().duration * (m_removedChunksToKeepCount - m_chunks.size()); //estimation of length of next chunks to skip (not yet existing)
                }
                m_timestampToBlock.push( std::make_pair(
                    m_chunks.front().startTimestamp,
                    keepChunkDataTillTimestamp ) );

                m_totalPlaylistDuration -= m_chunks.front().duration;

                NX_LOG(lit("LivePlaylistManager. Removing chunk (%1:%2). Total chunks left %3").
                    arg(m_chunks.front().startTimestamp).arg(m_chunks.front().duration).arg(m_chunks.size()), cl_logDEBUG2);
                m_chunks.pop_front();
            }

            NX_ASSERT( !m_chunks.empty() );

            while( !m_timestampToBlock.empty() && (m_timestampToBlock.front().second < m_chunks.front().startTimestamp) )
            {
                m_timestampToBlock.pop();
                //locking chunk data in media data in conformance with draft-pantos-http-live-streaming-10
                m_mediaStreamCache->moveBlocking(
                    m_blockID,
                    !m_timestampToBlock.empty()
                        ? m_timestampToBlock.front().first
                        : m_chunks.front().startTimestamp );
            }
        }
    }

    if( m_currentChunk.mediaSequence == 0 )
    {
        //creating new chunk
        m_currentChunk.startTimestamp = currentPacketTimestampUSec;
        m_currentChunk.mediaSequence = ++m_mediaSequence;
        m_currentChunk.duration = 0;
    }

    if( m_blockID == -1 )
        m_blockID = m_mediaStreamCache->blockData( currentPacketTimestampUSec );
}

} // namespace hls
} // namespace mediaserver
} // namespace nx
