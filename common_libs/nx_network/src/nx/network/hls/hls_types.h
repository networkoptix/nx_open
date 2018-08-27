#pragma once

#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>

#include <nx/utils/std/optional.h>
#include <nx/utils/url.h>

namespace nx::network::hls {

class NX_NETWORK_API Chunk
{
public:
    double duration;
    nx::utils::Url url;
    /** If true, there is discontinuity between this chunk and previous one. */
    bool discontinuity;
    /** #EXT-X-PROGRAM-DATE-TIME tag. */
    std::optional<QDateTime> programDateTime;

    Chunk();
};

class NX_NETWORK_API Playlist
{
public:
    unsigned int mediaSequence;
    bool closed;
    std::vector<Chunk> chunks;
    std::optional<bool> allowCache;

    Playlist();

    QByteArray toString() const;
};

class NX_NETWORK_API VariantPlaylistData
{
public:
    nx::utils::Url url;
    std::optional<int> bandwidth;
};

class NX_NETWORK_API VariantPlaylist
{
public:
    std::vector<VariantPlaylistData> playlists;

    QByteArray toString() const;
};

/** iOS device starts playing first chunk when downloading third one. */
static const int MIN_CHUNKS_REQUIRED_TO_START_PLAYBACK = 3;

} // namespace nx::network::hls
