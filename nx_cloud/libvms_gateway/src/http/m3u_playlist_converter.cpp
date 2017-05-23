#include "m3u_playlist_converter.h"

#include <nx/network/m3u/m3u_playlist.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx {
namespace cloud {
namespace gateway {

M3uPlaylistConverter::M3uPlaylistConverter(const nx::String& targetHost):
    m_targetHost(targetHost)
{
}

nx_http::BufferType M3uPlaylistConverter::convert(
    nx_http::BufferType originalBody)
{
    using namespace nx::network::m3u;

    Playlist playlist;
    if (!playlist.parse(originalBody))
        return nx_http::BufferType();

    for (auto& entry: playlist.entries)
    {
        if (entry.type != EntryType::location)
            continue;
        QUrl url(entry.value);
        url.setPath(nx::network::url::normalizePath(
            lm("/%1/%2").arg(m_targetHost).arg(url.path())));
        entry.value = url.toString().toUtf8();
    }

    return playlist.toString();
}

} // namespace gateway
} // namespace cloud
} // namespace nx
