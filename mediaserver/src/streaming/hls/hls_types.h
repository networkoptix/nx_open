////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_TYPES_H
#define HLS_TYPES_H

#include <vector>

#include <QByteArray>
#include <QUrl>


namespace nx_hls
{
    class Chunk
    {
    public:
        double duration;
        QUrl url;

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
}

#endif  //HLS_TYPES_H
