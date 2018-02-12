#include "url_parse_helper.h"

#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>

namespace nx {
namespace network {
namespace url {

quint16 getDefaultPortForScheme(const QString& scheme)
{
    if (scheme.toLower() == nx_http::kUrlSchemeName)
        return 80;
    else if (scheme.toLower() == nx_http::kSecureUrlSchemeName)
        return 443;
    else if (scheme.toLower() == nx_rtsp::kUrlSchemeName)
        return 554;

    return 0;
}

SocketAddress getEndpoint(const QUrl& url)
{
    return SocketAddress(
        url.host(),
        static_cast<quint16>(url.port(getDefaultPortForScheme(url.scheme()))));
}

QString normalizePath(const QString& path)
{
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
