#pragma once

#include <vector>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QUrl>

namespace nx {
namespace mediaserver {
namespace hls {

class NX_NETWORK_API Chunk
{
public:
    double duration;
    QUrl url;
    /** If true, there is discontinuity between this chunk and previous one. */
    bool discontinuity;
    /** #EXT-X-PROGRAM-DATE-TIME tag. */
    boost::optional<QDateTime> programDateTime;

    Chunk();
};

class NX_NETWORK_API Playlist
{
public:
    unsigned int mediaSequence;
    bool closed;
    std::vector<Chunk> chunks;
    boost::optional<bool> allowCache;

    Playlist();

    QByteArray toString() const;
};

class NX_NETWORK_API VariantPlaylistData
{
public:
    QUrl url;
    boost::optional<int> bandwidth;
};

class NX_NETWORK_API VariantPlaylist
{
public:
    std::vector<VariantPlaylistData> playlists;

    QByteArray toString() const;
};

/** iOS device starts playing first chunk when downloading third one. */
static const int MIN_CHUNKS_REQUIRED_TO_START_PLAYBACK = 3;

} // namespace hls
} // namespace mediaserver
} // namespace nx
