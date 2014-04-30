////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_archive_playlist_manager.h"

#include <limits>

#include <QtCore/QMutexLocker>

#include <core/resource/security_cam_resource.h>
#include <device_plugins/server_archive/server_archive_delegate.h>


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
        m_archiveDelegate( nullptr ),
        m_delegate( nullptr )
    {
        m_archiveDelegate = camResource->createArchiveDelegate();
        if( !m_archiveDelegate )
            m_archiveDelegate = new QnServerArchiveDelegate(); // default value

        m_archiveDelegate->setQuality(MEDIA_Quality_High, true);
        m_delegate = new QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr(m_archiveDelegate));
        m_delegate->setRange( m_startTimestamp, endTimestamp(), targetDurationUsec );

        m_prevChunkEndTimestamp = getNextStepArchiveTimestamp();
    }

    ArchivePlaylistManager::~ArchivePlaylistManager()
    {
        delete m_delegate;
        m_delegate = nullptr;

        delete m_archiveDelegate;
        m_archiveDelegate = nullptr;
    }

    unsigned int ArchivePlaylistManager::generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached ) const
    {
        QMutexLocker lk( &m_mutex );

        std::copy( m_chunks.begin(), m_chunks.end(), std::back_inserter(*chunkList) );
        if( endOfStreamReached )
            *endOfStreamReached = m_eof;
        return m_chunks.size();
    }

    bool ArchivePlaylistManager::addOneMoreChunk()
    {
        if( m_eof )
            return false;

        const qint64 nextChunkEndTimestamp = getNextStepArchiveTimestamp();
        if( nextChunkEndTimestamp == -1 )
        {
            //end of archive reached
            //TODO: #ak end of archive is moving forward constantly
            m_eof = true;
            return false;
        }

        AbstractPlaylistManager::ChunkData chunkData;
        chunkData.mediaSequence = ++m_chunkMediaSequence;
        chunkData.startTimestamp = m_prevChunkEndTimestamp;
        chunkData.duration = m_targetDurationUsec;   //TODO: #ak get real chunk duration
        m_totalPlaylistDuration += chunkData.duration;
        m_chunks.push_back( chunkData );

        if( m_chunks.size() > m_maxChunkNumberInPlaylist )
        {
            m_totalPlaylistDuration -= m_chunks.front().duration;
            m_chunks.pop_front();
        }

        m_prevChunkEndTimestamp = nextChunkEndTimestamp;

        return true;
    }

    qint64 ArchivePlaylistManager::getNextStepArchiveTimestamp() const
    {
        QnAbstractMediaDataPtr nextData = m_delegate->getNextData();
        if( !nextData )
            return -1;
        return nextData->timestamp;
    }

    qint64 ArchivePlaylistManager::endTimestamp() const
    {
        //TODO/IMPL returning max archive timestamp
        return std::numeric_limits<qint64>::max();
    }
}
