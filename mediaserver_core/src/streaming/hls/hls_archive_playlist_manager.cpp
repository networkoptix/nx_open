////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_archive_playlist_manager.h"

#include <limits>

#include <QtCore/QMutexLocker>

#include <core/resource/security_cam_resource.h>
#include <plugins/resource/server_archive/server_archive_delegate.h>

#include "streaming/streaming_chunk_cache_key.h"


namespace nx_hls
{
    //!Such a huge GOP is possible in case of low fps stream
    static const qint64 MAX_GOP_LENGTH_SEC = 100;
    static const qint64 USEC_IN_MSEC = 1000;
    static const qint64 MSEC_IN_SEC = 1000;

    ArchivePlaylistManager::ArchivePlaylistManager(
        const QnSecurityCamResourcePtr& camResource,
        qint64 startTimestamp,
        unsigned int maxChunkNumberInPlaylist,
        qint64 targetDurationUsec,
        MediaQuality streamQuality )
    :
        m_camResource( camResource ),
        m_startTimestamp( startTimestamp ),
        m_maxChunkNumberInPlaylist( maxChunkNumberInPlaylist ),
        m_targetDurationUsec( targetDurationUsec ),
        m_getNextIFrameLoopMaxSize( MAX_GOP_LENGTH_SEC * USEC_IN_MSEC * MSEC_IN_SEC / m_targetDurationUsec ),
        m_streamQuality( streamQuality ),
        m_totalPlaylistDuration( 0 ),
        m_prevChunkEndTimestamp( 0 ),
        m_eof( false ),
        m_chunkMediaSequence( 0 ),
        m_timerCorrection( 0 ),
        m_initialPlaylistCreated( false ),
        m_prevGeneratedChunkDuration( 0 ),
        m_discontinuityDetected( false )
    {
        Q_ASSERT( m_maxChunkNumberInPlaylist > 0 );
    }

    ArchivePlaylistManager::~ArchivePlaylistManager()
    {
    }

    bool ArchivePlaylistManager::initialize()
    {
        QnAbstractArchiveDelegatePtr archiveDelegate( m_camResource->createArchiveDelegate() );
        if( !archiveDelegate )
        {
            archiveDelegate = QnAbstractArchiveDelegatePtr(new QnServerArchiveDelegate()); // default value
            if( !archiveDelegate->open(m_camResource) )
                return false;
        }

        if( !archiveDelegate->setQuality( m_streamQuality, true ) )
            return false;
        m_delegate.reset( new QnThumbnailsArchiveDelegate(archiveDelegate) );
        m_delegate->setRange( m_startTimestamp, std::numeric_limits<qint64>::max(), m_targetDurationUsec );

        const QnAbstractMediaDataPtr& nextData = m_delegate->getNextData();
        if( !nextData )
        {
            m_prevChunkEndTimestamp = -1;
            return false;
        }
        m_prevChunkEndTimestamp = nextData->timestamp;
        m_currentArchiveChunk = m_delegate->getLastUsedChunkInfo();

        m_initialPlaylistCreated = false;
        return true;
    }

    size_t ArchivePlaylistManager::generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached ) const
    {
        QMutexLocker lk( &m_mutex );

        const_cast<ArchivePlaylistManager*>(this)->generateChunksIfNeeded();

        std::copy( m_chunks.cbegin(), m_chunks.cend(), std::back_inserter(*chunkList) );
        if( endOfStreamReached )
            *endOfStreamReached = m_eof;
        return m_chunks.size();
    }

    int ArchivePlaylistManager::getMaxBitrate() const
    {
        //TODO/HLS: #ak
        return -1;
    }

    void ArchivePlaylistManager::generateChunksIfNeeded()
    {
        if( !m_initialPlaylistCreated )
        {
            //creating initial playlist
            for( size_t i = 0; i < m_maxChunkNumberInPlaylist; ++i )
            {
                if( !addOneMoreChunk() )
                    break;
            }
            m_initialPlaylistCreated = true;
            m_playlistUpdateTimer.start();
            m_timerCorrection = 0;
        }
        else
        {
            if( (m_playlistUpdateTimer.elapsed() * USEC_IN_MSEC - m_timerCorrection) > m_prevGeneratedChunkDuration )
            {
                if( addOneMoreChunk() )
                    m_timerCorrection += m_prevGeneratedChunkDuration;
            }
        }
    }

    bool ArchivePlaylistManager::addOneMoreChunk()
    {
        if( m_eof )
            return false;

        QnAbstractMediaDataPtr nextData;
        for( int i = 0; i < m_getNextIFrameLoopMaxSize; ++i )
        {
            nextData = m_delegate->getNextData();
            if( !nextData )
            {
                //end of archive reached
                //TODO/HLS: #ak end of archive is moving forward constantly, so need just imply some delay 
                m_eof = true;
                return false;
            }
            if( nextData->timestamp != m_prevChunkEndTimestamp )
                break;
        }
        Q_ASSERT( nextData->timestamp != m_prevChunkEndTimestamp );
        if( nextData->timestamp == m_prevChunkEndTimestamp )
            return false;
        const qint64 currentChunkEndTimestamp = nextData->timestamp;

        AbstractPlaylistManager::ChunkData chunkData;
        chunkData.mediaSequence = ++m_chunkMediaSequence;
        chunkData.startTimestamp = m_prevChunkEndTimestamp;
        chunkData.discontinuity = m_discontinuityDetected;
        if( nextData->flags & QnAbstractMediaData::MediaFlags_BOF )
        {
            //gap in archive detected
            if( m_prevChunkEndTimestamp >= m_currentArchiveChunk.startTimeUsec && 
                m_prevChunkEndTimestamp < (m_currentArchiveChunk.startTimeUsec + m_currentArchiveChunk.durationUsec) )
            {
                chunkData.duration = m_currentArchiveChunk.durationUsec - (m_prevChunkEndTimestamp - m_currentArchiveChunk.startTimeUsec);
                Q_ASSERT( chunkData.duration > 0 );
            }
            else
            {
                //TODO/HLS: #ak some correction is required to call addOneMoreChunk() at appropriate time
                chunkData.duration = m_targetDurationUsec;
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
        m_chunks.push_back( chunkData );

        if( m_chunks.size() > m_maxChunkNumberInPlaylist )
        {
            m_totalPlaylistDuration -= m_chunks.front().duration;
            m_chunks.pop_front();
        }

        m_prevChunkEndTimestamp = currentChunkEndTimestamp;
        m_prevGeneratedChunkDuration = chunkData.duration;
        m_currentArchiveChunk = m_delegate->getLastUsedChunkInfo();

        return true;
    }

    qint64 ArchivePlaylistManager::endTimestamp() const
    {
        //returning max archive timestamp
        return m_delegate->endTime();
    }
}
