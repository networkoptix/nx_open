// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "m3u_playlist_converter.h"

#include <nx/network/m3u/m3u_playlist.h>
#include <nx/utils/log/log.h>

namespace nx::network::http::server::proxy {

M3uPlaylistConverter::M3uPlaylistConverter(
    const AbstractUrlRewriter& urlRewriter,
    const nx::utils::Url& proxyHostUrl,
    const std::string& targetHost)
    :
    m_urlRewriter(urlRewriter),
    m_proxyHostUrl(proxyHostUrl),
    m_targetHost(targetHost)
{
}

nx::Buffer M3uPlaylistConverter::convert(
    nx::Buffer originalBody)
{
    using namespace nx::network::m3u;

    Playlist playlist;
    if (!playlist.parse(originalBody))
        return nx::Buffer();

    for (auto& entry: playlist.entries)
    {
        if (entry.type != EntryType::location)
            continue;

        nx::utils::Url url(entry.value);
        url = m_urlRewriter.originalResourceUrlToProxyUrl(url, m_proxyHostUrl, m_targetHost);
        NX_VERBOSE(this, "Replacing URL %1 with %2", entry.value, url);
        entry.value = url.toString().toStdString();
    }

    return nx::Buffer(playlist.toString());
}

} // namespace nx::network::http::server::proxy
