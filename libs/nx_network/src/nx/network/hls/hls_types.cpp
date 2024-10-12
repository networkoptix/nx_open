// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hls_types.h"

#include <cmath>

#include <QtCore/QTimeZone>

#include <nx/utils/log/log.h>

namespace nx::network::hls {

QString timeZoneOffsetString(QDateTime dt1)
{
    QDateTime dt2 = dt1.toUTC();
    dt1.setTimeZone(QTimeZone::UTC);
    const int offsetSeconds = dt2.secsTo(dt1);

    const QChar sign = offsetSeconds >= 0 ? '+' : '-';
    const int hours = std::abs(offsetSeconds) / 3600;
    const int minutes = (std::abs(offsetSeconds) % 3600) / 60;
    return QString("%1%2:%3").arg(sign).arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0'));
}

std::string Playlist::toString() const
{
    int targetDuration = 0;
    for (const auto& chunk: chunks)
    {
        if (std::ceil(chunk.duration) > targetDuration)
            targetDuration = std::ceil(chunk.duration);
    }

    std::string playlistStr;
    playlistStr +=
        "#EXTM3U\r\n"
        "#EXT-X-VERSION:3\r\n";
    playlistStr += "#EXT-X-TARGETDURATION:" + std::to_string(targetDuration) + "\r\n";
    playlistStr += "#EXT-X-MEDIA-SEQUENCE:" + std::to_string(mediaSequence) + "\r\n";
    if (allowCache)
    {
        playlistStr += "#EXT-X-ALLOW-CACHE:";
        playlistStr += allowCache ? "YES" : "NO";
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
            QString dateTime = chunk.programDateTime->toString(Qt::ISODateWithMs);
            dateTime += timeZoneOffsetString(*chunk.programDateTime);
            NX_VERBOSE(this, "ProgramDateTime: converting date to ISO string. source value=%1, result=%2",
                chunk.programDateTime->toMSecsSinceEpoch(), dateTime);

            playlistStr += "#EXT-X-PROGRAM-DATE-TIME:";
            playlistStr += dateTime.toStdString(); //< data/time.
            playlistStr += "\r\n";
        }
        playlistStr += "#EXTINF:" + std::to_string(chunk.duration) + ",\r\n";

        // NOTE: Reporting only path if host not specified.
        playlistStr += chunk.url.host().isEmpty()
            ? chunk.url.path().toStdString() + "?" + chunk.url.query(QUrl::FullyEncoded).toStdString()
            : chunk.url.toString(QUrl::FullyEncoded).toStdString();
        playlistStr += "\r\n";
    }

    if (closed)
        playlistStr += "#EXT-X-ENDLIST\r\n";

    return playlistStr;
}

//-------------------------------------------------------------------------------------------------

std::string VariantPlaylist::toString() const
{
    std::string str;
    str += "#EXTM3U\r\n";

    for (const auto& playlist: playlists)
    {
        str += "#EXT-X-STREAM-INF:";
        if (playlist.bandwidth)
            str += "BANDWIDTH=" + std::to_string(*playlist.bandwidth);
        str += "\r\n";
        str += playlist.url.host().isEmpty()
            ? playlist.url.path().toStdString() + "?" +
                playlist.url.query(QUrl::FullyEncoded).toStdString()    //< Reporting only path if host not specified.
            : playlist.url.toString(QUrl::FullyEncoded).toStdString();
        str += "\r\n";
    }

    return str;
}

} // namespace nx::network::hls
