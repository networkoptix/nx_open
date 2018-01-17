#include "m3u_playlist.h"

#include <nx/network/http/line_splitter.h>

namespace nx {
namespace network {
namespace m3u {

Entry::Entry(
    EntryType type,
    nx::String value)
    :
    type(type),
    value(value)
{
}

bool Entry::operator==(const Entry& right) const
{
    return type == right.type && value == right.value;
}

//-------------------------------------------------------------------------------------------------

bool Playlist::parse(const nx::String& str)
{
    nx::network::http::StringLineIterator lineSplitter(str);
    while (auto line = lineSplitter.next())
    {
        const auto trimmedLine = line->trimmed();
        if (trimmedLine.isEmpty())
            continue;

        const auto entryType =
            trimmedLine.startsWith('#') ? EntryType::directive : EntryType::location;
        entries.push_back(Entry(entryType, (QByteArray)trimmedLine));
    }

    return true;
}

nx::String Playlist::toString() const
{
    nx::String result;
    for (const auto& entry: entries)
    {
        result.append(entry.value);
        result.append("\r\n");
    }

    return result;
}

bool Playlist::operator==(const Playlist& right) const
{
    return entries == right.entries;
}

} // namespace m3u
} // namespace network
} // namespace nx
