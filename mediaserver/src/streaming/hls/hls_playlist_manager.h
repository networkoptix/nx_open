////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_PLAYLIST_MANAGER_H
#define HLS_PLAYLIST_MANAGER_H

#include <vector>


namespace nx_hls
{
    class AbstractPlaylistManager
    {
    public:
        class ChunkData
        {
        public:
            //!internal timestamp (micros). Not a calendar time
            quint64 startTimestamp;
            //!in micros
            quint64 duration;
            //!sequential number of this chunk
            unsigned int mediaSequence;
            //!if true, there is discontinuity between this chunk and preceding one
            bool discontinuity;

            ChunkData();
            ChunkData(
                quint64 _startTimestamp,
                quint64 _duration,
                unsigned int _mediaSequence,
                bool _discontinuity = false );
        };

        //!Generates chunks and appends them to \a chunkList
        /*!
            \param endOfStreamReached Can be NULL
            \return Number of chunks generated
        */
        virtual size_t generateChunkList(
            std::vector<ChunkData>* const chunkList,
            bool* const endOfStreamReached ) const = 0;
    };
}

#endif  //HLS_PLAYLIST_MANAGER_H
