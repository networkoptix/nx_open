////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_archive_playlist_manager.h"


namespace nx_hls
{
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
        //TODO/IMPL
        return 0;
    }
}
