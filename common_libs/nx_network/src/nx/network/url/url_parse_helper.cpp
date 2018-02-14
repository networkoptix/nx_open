#include "url_parse_helper.h"

#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>

namespace nx {
namespace network {
namespace url {

quint16 getDefaultPortForScheme(const QString& scheme)
{
    if (scheme.toLower() == nx::network::http::kUrlSchemeName)
        return 80;
    else if (scheme.toLower() == nx::network::http::kSecureUrlSchemeName)
        return 443;
    else if (scheme.toLower() == nx_rtsp::kUrlSchemeName)
        return 554;

    return 0;
}

SocketAddress getEndpoint(const nx::utils::Url& url)
{
    return SocketAddress(
        url.host(),
        static_cast<quint16>(url.port(getDefaultPortForScheme(url.scheme()))));
}

std::string normalizePath(std::string path)
{
    // TODO: #ak Remove "..".

    for (std::string::size_type pos = 0;;)
    {
        pos = path.find("//", pos);
        if (pos == std::string::npos)
            break;
        path.replace(pos, 2U, "/");
    }

    return path;
}

QString normalizePath(const QString& path)
    if (path.indexOf("//") == -1)
        return path;

    QString normalizedPath;
    normalizedPath.reserve(path.size());

    QChar prevCh = ' '; //< Anything but not /.
    for (const auto& ch: path)
    {
        if (ch == L'/' && prevCh == L'/')
            continue;
        prevCh = ch;
        normalizedPath.push_back(ch);
    }

    return normalizedPath;
}

QString joinPath(const QString& left, const QString& right)
{
    return normalizePath(lm("%1/%2").args(left, right).toQString());
}

} // namespace url
} // namespace network
} // namespace nx
