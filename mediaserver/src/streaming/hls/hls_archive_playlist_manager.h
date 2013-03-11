////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_ARCHIVE_PLAYLIST_MANAGER_H
#define HLS_ARCHIVE_PLAYLIST_MANAGER_H

#include "hls_playlist_manager.h"


namespace nx_hls
{
    //!Generates sliding chunk list for archive playback
    /*!
        It is necessary to make archive playlist sliding since archive can be very long and playlist would be quite large (~ 25Mb for a month archive)
    */
    class ArchivePlaylistManager
    :
        public AbstractPlaylistManager
    {
    public:
        ArchivePlaylistManager(
            quint64 startTimestamp,
            unsigned int maxChunkNumberInPlaylist );

        //!Implementantion of AbstractPlaylistManager::generateChunkList
        virtual unsigned int generateChunkList(
            std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
            bool* const endOfStreamReached ) const;

    private:
        quint64 m_startTimestamp;
        unsigned int m_maxChunkNumberInPlaylist;
    };
}

#endif  //HLS_ARCHIVE_PLAYLIST_MANAGER_H
