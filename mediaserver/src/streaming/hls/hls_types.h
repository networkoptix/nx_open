////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_TYPES_H
#define HLS_TYPES_H

#include <vector>

#include <QByteArray>
#include <QUrl>


//TODO/IMPL move OptionalField somewhere else...
template<class T>
    class OptionalField
{
public:
    bool present;
    T value;

    OptionalField()
    :
        present( false ),
        value( T() )
    {
    }

    OptionalField( const T& val )
    :
        present( true ),
        value( val )
    {
    }

    operator T() const
    {
        return value;
    }

    operator T()
    {
        return value;
    }
};

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
        OptionalField<unsigned int> bandwidth;
    };

    class VariantPlaylist
    {
    public:
        std::vector<VariantPlaylistData> playlists;

        QByteArray toString() const;
    };
}

#endif  //HLS_TYPES_H
