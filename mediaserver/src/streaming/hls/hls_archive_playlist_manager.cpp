////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_archive_playlist_manager.h"

#include <limits>

#include <QtCore/QMutexLocker>

#include <core/resource/security_cam_resource.h>
#include <device_plugins/server_archive/server_archive_delegate.h>


static const qint64 USEC_IN_MSEC = 1000;

namespace nx_hls
{
    ArchivePlaylistManager::ArchivePlaylistManager(
        const QnSecurityCamResourcePtr& camResource,
        quint64 startTimestamp,
        unsigned int maxChunkNumberInPlaylist,
        quint64 targetDurationUsec )
    :
        m_camResource( camResource ),
        m_startTimestamp( startTimestamp ),
        m_maxChunkNumberInPlaylist( maxChunkNumberInPlaylist ),
        m_targetDurationUsec( targetDurationUsec ),
        m_totalPlaylistDuration( 0 ),
        m_prevChunkEndTimestamp( 0 ),
        m_eof( false ),
        m_chunkMediaSequence( 0 ),
        m_delegate( nullptr ),
        m_timerCorrection( 0 ),
        m_initialPlaylistCreated( false ),
        m_prevGeneratedChunkDuration( 0 ),
        m_discontinuityDetected( false )
    {
        assert( m_maxChunkNumberInPlaylist > 0 );
    }

    ArchivePlaylistManager::~ArchivePlaylistManager()
    {
        delete m_delegate;
        m_delegate = nullptr;
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

        if( !archiveDelegate->setQuality(MEDIA_Quality_High, true) )    //TODO/HLS: #ak set proper quality
            return false;
        m_delegate = new QnThumbnailsArchiveDelegate(archiveDelegate);
        m_delegate->setRange( m_startTimestamp, std::numeric_limits<qint64>::max(), m_targetDurationUsec );

        QnAbstractMediaDataPtr nextData = m_delegate->getNextData();
        if( !nextData )
        {
            m_prevChunkEndTimestamp = -1;
            return false;
        }
        m_prevChunkEndTimestamp = nextData->timestamp;

        m_initialPlaylistCreated = false;
        return true;
    }

    unsigned int ArchivePlaylistManager::generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached ) const
    {
        QMutexLocker lk( &m_mutex );

        const_cast<ArchivePlaylistManager*>(this)->generateChunksIfNeeded();

        std::copy( m_chunks.begin(), m_chunks.end(), std::back_inserter(*chunkList) );
        if( endOfStreamReached )
            *endOfStreamReached = m_eof;
        return m_chunks.size();
    }

    void ArchivePlaylistManager::generateChunksIfNeeded()
    {
        if( !m_initialPlaylistCreated )
        {
            //creating initial playlist
            for( int i = 0; i < m_maxChunkNumberInPlaylist; ++i )
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

        QnAbstractMediaDataPtr nextData = m_delegate->getNextData();
        if( !nextData )
        {
            //end of archive reached
            //TODO: #ak end of archive is moving forward constantly
            m_eof = true;
            return false;
        }
        const qint64 currentChunkEndTimestamp = nextData->timestamp;

        AbstractPlaylistManager::ChunkData chunkData;
        chunkData.mediaSequence = ++m_chunkMediaSequence;
        chunkData.startTimestamp = m_prevChunkEndTimestamp;
        chunkData.duration = (nextData->flags & QnAbstractMediaData::MediaFlags_BOF)
            ? m_targetDurationUsec                                  //taking into account gaps in archive
            : currentChunkEndTimestamp - chunkData.startTimestamp;
        chunkData.discontinuity = m_discontinuityDetected;
        m_discontinuityDetected = (nextData->flags & QnAbstractMediaData::MediaFlags_BOF) > 0;
        m_totalPlaylistDuration += chunkData.duration;
        m_chunks.push_back( chunkData );

        //TODO: #ak timer correction is required in case of discontinuity

        if( m_chunks.size() > m_maxChunkNumberInPlaylist )
        {
            m_totalPlaylistDuration -= m_chunks.front().duration;
            m_chunks.pop_front();
        }

        m_prevChunkEndTimestamp = currentChunkEndTimestamp;
        m_prevGeneratedChunkDuration = chunkData.duration;

        return true;
    }

    qint64 ArchivePlaylistManager::endTimestamp() const
    {
        //returning max archive timestamp
        //return std::numeric_limits<qint64>::max();
        return m_delegate->endTime();
    }
}
