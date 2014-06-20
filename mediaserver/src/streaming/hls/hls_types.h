////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_TYPES_H
#define HLS_TYPES_H

#include <vector>

#include <boost/optional.hpp>

#include <QByteArray>
#include <QDateTime>
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
        //!#EXT-X-PROGRAM-DATE-TIME tag
        boost::optional<QDateTime> programDateTime;

        Chunk();
    };

    class Playlist
    {
    public:
        unsigned int mediaSequence;
        bool closed;
        std::vector<Chunk> chunks;
        boost::optional<bool> allowCache;

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
