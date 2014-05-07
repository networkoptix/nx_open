////////////////////////////////////////////////////////////
// 21 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_live_playlist_manager.h"

#include <QtCore/QMutexLocker>


namespace nx_hls
{
    static const double DEFAULT_TARGET_DURATION_USEC = 10*1000*1000;

    HLSLivePlaylistManager::HLSLivePlaylistManager(
        MediaStreamCache* const mediaStreamCache,
        MediaIndex* const mediaIndex )
    :
        m_mediaStreamCache( mediaStreamCache ),
        m_mediaIndex( mediaIndex ),
        m_targetDurationUSec( DEFAULT_TARGET_DURATION_USEC ),
        m_prevTimestamp( 0 ),
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
    unsigned int HLSLivePlaylistManager::generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached ) const
    {
        QMutexLocker lk( &m_mutex );
        std::copy( m_chunks.begin(), m_chunks.end(), std::back_inserter(*chunkList) );
        if( endOfStreamReached )
            *endOfStreamReached = false;
        return m_chunks.size();
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

                //TODO/IMPL/HLS try not to exceed m_targetDurationUSec
                m_chunks.push_back( m_currentChunk );
                m_totalPlaylistDuration += m_currentChunk.duration;
                m_currentChunk.mediaSequence = 0;   //using media sequence 0 for NULL chunk

                //checking whether some chunks need to be removed
                    //The server MUST NOT remove a media segment from the Playlist file if
                    //   the duration of the Playlist file minus the duration of the segment
                    //   is less than three times the target duration.
                while( m_totalPlaylistDuration - m_chunks.front().duration > m_targetDurationUSec * 3 )
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

                while( !m_timestampToBlock.empty() && m_timestampToBlock.front().second <= m_chunks.front().startTimestamp )
                {
                    m_timestampToBlock.pop();
                    Q_ASSERT( !m_timestampToBlock.empty() );
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

            if( m_chunks.empty() )
                m_blockID = m_mediaStreamCache->blockData( currentPacketTimestampUSec );
        }
    }
}
