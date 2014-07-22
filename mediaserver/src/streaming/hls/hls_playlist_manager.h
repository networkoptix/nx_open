////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_PLAYLIST_MANAGER_H
#define HLS_PLAYLIST_MANAGER_H

#include <memory>
#include <vector>

#include <boost/optional.hpp>


namespace nx_hls
{
    class AbstractPlaylistManager
    {
    public:
        class ChunkData
        {
        public:
            boost::optional<QString> alias;
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
            ChunkData(
                const QString& _alias,
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
        //!Returns maximum stream bitrate in bps
        virtual int getMaxBitrate() const = 0;
    };

    //!Using std::shared_ptr for \a std::shared_ptr::unique()
    typedef std::shared_ptr<AbstractPlaylistManager> AbstractPlaylistManagerPtr;
}

#endif  //HLS_PLAYLIST_MANAGER_H
