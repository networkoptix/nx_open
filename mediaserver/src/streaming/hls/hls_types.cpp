////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_types.h"

#include <cmath>


namespace nx_hls
{
    Chunk::Chunk()
    :
        duration( 0 ),
        discontinuity( false )
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
            if( std::ceil(chunks[i].duration) > targetDuration )
                targetDuration = std::ceil(chunks[i].duration);
        }

        QByteArray playlistStr;
        playlistStr += 
            "#EXTM3U\r\n"
            "#EXT-X-VERSION:3\r\n"; //TODO/HLS: #ak really need version 3?
        playlistStr += "#EXT-X-TARGETDURATION:"+QByteArray::number(targetDuration)+"\r\n";
        playlistStr += "#EXT-X-MEDIA-SEQUENCE:"+QByteArray::number(mediaSequence)+"\r\n";
        playlistStr += "\r\n";

        for( std::vector<Chunk>::size_type
            i = 0;
            i < chunks.size();
            ++i )
        {
            if( chunks[i].discontinuity )
                playlistStr += "#EXT-X-DISCONTINUITY\r\n";
            playlistStr += "#EXTINF:"+QByteArray::number(chunks[i].duration, 'f', 3)+",\r\n";
            playlistStr += chunks[i].url.toString().toLatin1()+"\r\n";
        }
        
        if( closed )
            playlistStr += "#EXT-X-ENDLIST\r\n";

        return playlistStr;
    }


    QByteArray VariantPlaylist::toString() const
    {
        QByteArray str;
        str += "#EXTM3U\r\n";

        for( std::vector<VariantPlaylistData>::size_type
            i = 0;
            i < playlists.size();
            ++i )
        {
            str += "#EXT-X-STREAM-INF:";
            if( playlists[i].bandwidth.present )
                str += "BANDWIDTH="+QByteArray::number(playlists[i].bandwidth.value);
            str += "\r\n";
            str += playlists[i].url.toString().toLatin1();
            str += "\r\n";
        }

        return str;
    }
}
