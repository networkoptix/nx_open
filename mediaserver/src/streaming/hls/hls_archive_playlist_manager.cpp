////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_archive_playlist_manager.h"

#include <limits>


namespace nx_hls
{
    //TODO/IMPL control end of archive

    ArchivePlaylistManager::ArchivePlaylistManager(
        quint64 startTimestamp,
        unsigned int maxChunkNumberInPlaylist )
    :
        m_startTimestamp( startTimestamp ),
        m_maxChunkNumberInPlaylist( maxChunkNumberInPlaylist )
    {
    }

    unsigned int ArchivePlaylistManager::generateChunkList(
        std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const endOfStreamReached ) const
    {
        Q_ASSERT( false );
        //TODO/IMPL
        return 0;
    }

    quint64 ArchivePlaylistManager::endTimestamp() const
    {
        //TODO/IMPL
        return std::numeric_limits<quint64>::max();
    }
}
