// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QtCore/QDateTime>

#include <nx/utils/url.h>

namespace nx::network::hls {

class NX_NETWORK_API Chunk
{
public:
    double duration = 0.0;
    nx::utils::Url url;
    /** If true, there is discontinuity between this chunk and previous one. */
    bool discontinuity = false;
    /** #EXT-X-PROGRAM-DATE-TIME tag. */
    std::optional<QDateTime> programDateTime;
};

class NX_NETWORK_API Playlist
{
public:
    unsigned int mediaSequence = 0;
    bool closed = false;
    std::vector<Chunk> chunks;
    std::optional<bool> allowCache;

    std::string toString() const;
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

    std::string toString() const;
};

/** iOS device starts playing first chunk when downloading third one. */
static constexpr int MIN_CHUNKS_REQUIRED_TO_START_PLAYBACK = 3;

} // namespace nx::network::hls
