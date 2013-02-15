////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_types.h"


namespace nx_hls
{
    Chunk::Chunk()
    :
        duration( 0 )
    {
    }

    Playlist::Playlist()
    :
        mediaSequence( 0 ),
        closed( false )
    {
    }

    QByteArray Playlist::toString() const
    {
        int targetDuration = 0;
        for( std::vector<Chunk>::size_type
            i = 0;
            i < chunks.size();
            ++i )
        {
            if( chunks[i].duration > targetDuration )
                targetDuration = chunks[i].duration;
        }

        QByteArray playlistStr;
        playlistStr += 
            "#EXTM3U\r\n"
            "#EXT-X-VERSION:3\r\n";
        playlistStr += "#EXT-X-TARGETDURATION:"+QByteArray::number(targetDuration)+"\r\n";
        playlistStr += "#EXT-X-MEDIA-SEQUENCE:"+QByteArray::number(mediaSequence)+"\r\n";
        playlistStr += "\r\n";

        for( std::vector<Chunk>::size_type
            i = 0;
            i < chunks.size();
            ++i )
        {
            playlistStr += "#EXTINF:"+QByteArray::number(chunks[i].duration, 'f', 3)+",\r\n";
            playlistStr += chunks[i].url.toString().toLatin1()+"\r\n";
        }
        
        if( closed )
            playlistStr += "#EXT-X-ENDLIST\r\n";

        return playlistStr;
    }
}
