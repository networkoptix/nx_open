#include "hls_types.h"

#include <cmath>

#include <QtCore/QTimeZone>

namespace nx_hls {

Chunk::Chunk():
    duration(0),
    discontinuity(false)
{
}

//-------------------------------------------------------------------------------------------------

Playlist::Playlist():
    mediaSequence(0),
    closed(false)
{
}

QByteArray Playlist::toString() const
{
    int targetDuration = 0;
    for (const auto& chunk: chunks)
    {
        if (std::ceil(chunk.duration) > targetDuration)
            targetDuration = std::ceil(chunk.duration);
    }

    QByteArray playlistStr;
    playlistStr +=
        "#EXTM3U\r\n"
        "#EXT-X-VERSION:3\r\n";
    playlistStr += "#EXT-X-TARGETDURATION:" + QByteArray::number(targetDuration) + "\r\n";
    playlistStr += "#EXT-X-MEDIA-SEQUENCE:" + QByteArray::number(mediaSequence) + "\r\n";
    if (allowCache)
    {
        playlistStr += "#EXT-X-ALLOW-CACHE:";
        playlistStr += allowCache.get() ? "YES" : "NO";
        playlistStr += "\r\n";
    }
    playlistStr += "\r\n";

    for (const auto& chunk: chunks)
    {
        if (chunk.discontinuity)
            playlistStr += "#EXT-X-DISCONTINUITY\r\n";
        // Generating string 2010-02-19T14:54:23.031+08:00, since QDateTime::setTimeSpec()
        //   and QDateTime::toString(Qt::ISODate) do not provide expected result.
        if (chunk.programDateTime)
        {
            const int tzOffset = chunk.programDateTime.get().timeZone().offsetFromUtc(chunk.programDateTime.get());
            playlistStr += "#EXT-X-PROGRAM-DATE-TIME:";
            playlistStr += chunk.programDateTime.get().toString(Qt::ISODate) + "."; //< data/time.
            playlistStr += QString::number((chunk.programDateTime.get().toMSecsSinceEpoch() % 1000)); // Milliseconds.
            playlistStr +=
                (tzOffset >= 0                                                                    //< Timezone.
                    ? ("+" + QTime(0, 0, 0).addSecs(tzOffset).toString(lit("hh:mm")))
                    : ("-" + QTime(0, 0, 0).addSecs(-tzOffset).toString(lit("hh:mm")))) +
                "\r\n";
        }
        playlistStr += "#EXTINF:" + QByteArray::number(chunk.duration, 'f', 3) + ",\r\n";
        playlistStr += chunk.url.host().isEmpty()
            ? chunk.url.path().toLatin1() + "?" + chunk.url.query(QUrl::FullyEncoded).toLatin1()    //< Reporting only path if host not specified.
            : chunk.url.toString(QUrl::FullyEncoded).toLatin1();
        playlistStr += "\r\n";
    }

    if (closed)
        playlistStr += "#EXT-X-ENDLIST\r\n";

    return playlistStr;
}

//-------------------------------------------------------------------------------------------------

QByteArray VariantPlaylist::toString() const
{
    QByteArray str;
    str += "#EXTM3U\r\n";

    for (const auto& playlist: playlists)
    {
        str += "#EXT-X-STREAM-INF:";
        if (playlist.bandwidth)
            str += "BANDWIDTH=" + QByteArray::number(playlist.bandwidth.get());
        str += "\r\n";
        str += playlist.url.host().isEmpty()
            ? playlist.url.path().toLatin1() + "?" +
                playlist.url.query(QUrl::FullyEncoded).toLatin1()    //< Reporting only path if host not specified.
            : playlist.url.toString(QUrl::FullyEncoded).toLatin1();
        str += "\r\n";
    }

    return str;
}

} // namespace nx_hls
