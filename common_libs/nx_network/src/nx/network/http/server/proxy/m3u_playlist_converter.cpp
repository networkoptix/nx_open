#include "m3u_playlist_converter.h"

#include <nx/network/m3u/m3u_playlist.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

M3uPlaylistConverter::M3uPlaylistConverter(
    const AbstractUrlRewriter& urlRewriter,
    const utils::Url& proxyHostUrl,
    const nx::String& targetHost)
    :
    m_urlRewriter(urlRewriter),
    m_proxyHostUrl(proxyHostUrl),
    m_targetHost(targetHost)
{
}

nx::network::http::BufferType M3uPlaylistConverter::convert(
    nx::network::http::BufferType originalBody)
{
    using namespace nx::network::m3u;

    Playlist playlist;
    if (!playlist.parse(originalBody))
        return nx::network::http::BufferType();

    for (auto& entry: playlist.entries)
    {
        if (entry.type != EntryType::location)
            continue;

        nx::utils::Url url(entry.value);
        url = m_urlRewriter.originalResourceUrlToProxyUrl(url, m_proxyHostUrl, m_targetHost);
        entry.value = url.toString().toUtf8();
    }

    return playlist.toString();
}

} // namespace proxy
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
