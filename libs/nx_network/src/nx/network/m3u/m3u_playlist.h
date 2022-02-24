// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/buffer.h>

namespace nx::network::m3u {

enum class EntryType
{
    unknown,
    directive,
    location,
};

struct NX_NETWORK_API Entry
{
    EntryType type = EntryType::unknown;
    std::string value;

    Entry() = default;

    Entry(
        EntryType type,
        std::string value);

    bool operator==(const Entry& right) const;
};

/**
 * See https://en.wikipedia.org/wiki/M3U.
 */
class NX_NETWORK_API Playlist
{
public:
    std::vector<Entry> entries;

    bool parse(const std::string_view& str);
    std::string toString() const;

    bool operator==(const Playlist& right) const;
};

} // namespace nx::network::m3u
