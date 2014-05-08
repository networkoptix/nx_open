////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_TYPES_H
#define HLS_TYPES_H

#include <vector>

#include <boost/optional.hpp>

#include <QByteArray>
#include <QUrl>


//!Consolidates hls implementation types
namespace nx_hls
{
    class Chunk
    {
    public:
        double duration;
        QUrl url;
        //!if true, there is discontinuity between this chunk and previous one
        bool discontinuity;

        Chunk();
    };

    class Playlist
    {
    public:
        unsigned int mediaSequence;
        bool closed;
        std::vector<Chunk> chunks;

        Playlist();

        QByteArray toString() const;
    };

    class VariantPlaylistData
    {
    public:
        QUrl url;
        boost::optional<int> bandwidth;
    };

    class VariantPlaylist
    {
    public:
        std::vector<VariantPlaylistData> playlists;

        QByteArray toString() const;
    };
}

#endif  //HLS_TYPES_H
