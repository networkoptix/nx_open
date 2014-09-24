////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_types.h"

#include <cmath>

#include <QTimeZone>


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
            "#EXT-X-VERSION:3\r\n";
        playlistStr += "#EXT-X-TARGETDURATION:"+QByteArray::number(targetDuration)+"\r\n";
        playlistStr += "#EXT-X-MEDIA-SEQUENCE:"+QByteArray::number(mediaSequence)+"\r\n";
        if( allowCache )
        {
            playlistStr += "#EXT-X-ALLOW-CACHE:";
            playlistStr += allowCache.get() ? "YES" : "NO";
            playlistStr += "\r\n";
        }
        playlistStr += "\r\n";

        for( std::vector<Chunk>::size_type
            i = 0;
            i < chunks.size();
            ++i )
        {
            if( chunks[i].discontinuity )
                playlistStr += "#EXT-X-DISCONTINUITY\r\n";
            //generating string 2010-02-19T14:54:23.031+08:00, since QDateTime::setTimeSpec() and QDateTime::toString(Qt::ISODate) do not provide expected result
            const int tzOffset = chunks[i].programDateTime.get().timeZone().offsetFromUtc( chunks[i].programDateTime.get() );
            if( chunks[i].programDateTime )
                playlistStr += "#EXT-X-PROGRAM-DATE-TIME:"+
                    chunks[i].programDateTime.get().toString(Qt::ISODate)+"."+                      // data/time
                    QString::number((chunks[i].programDateTime.get().toMSecsSinceEpoch() % 1000))+  //millis
                    (tzOffset >= 0                                                                    //timezone
                        ? ("+" + QTime( 0, 0, 0 ).addSecs( tzOffset ).toString( lit( "hh:mm" ) ))
                        : ("-" + QTime( 0, 0, 0 ).addSecs( -tzOffset ).toString( lit( "hh:mm" ) ))) +
                    "\r\n";
            playlistStr += "#EXTINF:"+QByteArray::number(chunks[i].duration, 'f', 3)+",\r\n";
            playlistStr += chunks[i].url.host().isEmpty()
                ? chunks[i].url.path().toLatin1()+"?"+chunks[i].url.query().toLatin1()    //reporting only path if host not specified
                : chunks[i].url.toString().toLatin1();
            playlistStr += "\r\n";
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
            if( playlists[i].bandwidth )
                str += "BANDWIDTH="+QByteArray::number(playlists[i].bandwidth.get());
            str += "\r\n";
            str += playlists[i].url.host().isEmpty()
                ? playlists[i].url.path().toLatin1()+"?"+playlists[i].url.query().toLatin1()    //reporting only path if host not specified
                : playlists[i].url.toString().toLatin1();
            str += "\r\n";
        }

        return str;
    }
}
