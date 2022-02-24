// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "m3u_playlist.h"

#include <nx/network/http/line_splitter.h>

#include <nx/utils/string.h>

namespace nx::network::m3u {

Entry::Entry(
    EntryType type,
    std::string value)
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

bool Playlist::parse(const std::string_view& str)
{
    nx::network::http::StringLineIterator lineSplitter(str);
    while (auto line = lineSplitter.next())
    {
        const auto trimmedLine = nx::utils::trim(*line);
        if (trimmedLine.empty())
            continue;

        const auto entryType =
            nx::utils::startsWith(trimmedLine, '#') ? EntryType::directive : EntryType::location;
        entries.push_back(Entry(entryType, std::string(trimmedLine)));
    }

    return true;
}

std::string Playlist::toString() const
{
    std::string result;
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

} // namespace nx::network::m3u
