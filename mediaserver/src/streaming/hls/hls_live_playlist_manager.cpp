////////////////////////////////////////////////////////////
// 21 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_live_playlist_manager.h"

#include <QtCore/QMutexLocker>

#include <utils/common/synctime.h>


namespace nx_hls
{
    static const size_t CHUNKS_IN_PLAYLIST = 3;

    HLSLivePlaylistManager::HLSLivePlaylistManager(
        MediaStreamCache* const mediaStreamCache,
        MediaIndex* const /*mediaIndex*/,
        quint64 targetDurationUSec )
    :
        m_mediaStreamCache( mediaStreamCache ),
        m_targetDurationUSec( targetDurationUSec ),
        m_mediaSequence( 0 ),
        m_totalPlaylistDuration( 0 ),
        m_blockID( -1 )
    {
        m_mediaStreamCache->addEventReceiver( this );
    }

    HLSLivePlaylistManager::~HLSLivePlaylistManager()
    {
        m_mediaStreamCache->unblockData( m_blockID );
        m_blockID = -1;
        m_mediaStreamCache->removeEventReceiver( this );
    }

    //!Same as \a generateChunkList, but returns \a chunksToGenerate last chunks of available data
    /*!
        \return Number of chunks generated
    */
    size_t HLSLivePlaylistManager::generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached ) const
    {
        QMutexLocker lk( &m_mutex );

        //NOTE commented code is a trick to minimize live delay with hls playback. But current implementation results in 
            //playback freezing and switching to lo-quality, since downloading is one with same speed as playing.
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

    int HLSLivePlaylistManager::getMaxBitrate() const
    {
        return m_mediaStreamCache->getMaxBitrate();
    }

    //!Implementation of AbstractMediaCacheEventReceiver::onKeyFrame
    void HLSLivePlaylistManager::onKeyFrame( quint64 currentPacketTimestampUSec )
    {
        QMutexLocker lk( &m_mutex );

        if( m_currentChunk.mediaSequence > 0 )
        {
            m_currentChunk.duration = currentPacketTimestampUSec - m_currentChunk.startTimestamp;
            if( m_currentChunk.duration >= m_targetDurationUSec )
            {
                const quint64 playlistDurationBak = m_totalPlaylistDuration;

                //if there is already chunk with same media sequence, replacing it...
                auto chunkIter = std::find_if( m_chunks.begin(), m_chunks.end(), 
                    [this]( const ChunkData& chunk ){ return chunk.mediaSequence == m_currentChunk.mediaSequence; } );
                if( chunkIter == m_chunks.end() )
                {
                    m_chunks.push_back( m_currentChunk );
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
                while( m_totalPlaylistDuration - m_chunks.front().duration > m_targetDurationUSec * CHUNKS_IN_PLAYLIST )
                //while( m_chunks.size() > CHUNKS_IN_PLAYLIST )
                {
                    //When the server removes a media segment from the Playlist, the
                    //   corresponding media URI SHOULD remain available to clients for a
                    //   period of time equal to the duration of the segment plus the duration
                    //   of the longest Playlist file distributed by the server containing
                    //   that segment.
                    m_timestampToBlock.push( std::make_pair(
                        m_chunks.front().startTimestamp,
                        m_chunks.front().startTimestamp + m_chunks.front().duration + playlistDurationBak ) );

                    m_totalPlaylistDuration -= m_chunks.front().duration;
                    m_chunks.pop_front();
                }

                Q_ASSERT( !m_chunks.empty() );

                while( !m_timestampToBlock.empty() && (m_timestampToBlock.front().second <= m_chunks.front().startTimestamp) )
                {
                    m_timestampToBlock.pop();
                    if( m_timestampToBlock.empty() )
                        break;
                    //locking chunk data in media data in conformance with draft-pantos-http-live-streaming-10
                    m_mediaStreamCache->moveBlocking(
                        m_blockID,
                        m_timestampToBlock.front().first );
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
}
