#pragma once

#include <vector>

#include "../buffer.h"

namespace nx {
namespace network {
namespace m3u {

enum class EntryType
{
    unknown,
    directive,
    location,
};

struct NX_NETWORK_API Entry
{
    EntryType type = EntryType::unknown;
    nx::String value;

    Entry() = default;

    Entry(
        EntryType type,
        nx::String value);

    bool operator==(const Entry& right) const;
};

/**
 * See https://en.wikipedia.org/wiki/M3U.
 */
class NX_NETWORK_API Playlist
{
public:
    std::vector<Entry> entries;

    bool parse(const nx::String& str);
    nx::String toString() const;

    bool operator==(const Playlist& right) const;
};

} // namespace m3u
} // namespace network
} // namespace nx
