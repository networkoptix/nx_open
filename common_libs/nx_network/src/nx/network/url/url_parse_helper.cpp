#include "url_parse_helper.h"

namespace nx {
namespace network {
namespace url {

quint16 getDefaultPortForScheme(const QString& scheme)
{
    if (scheme.toLower() == "http")
        return 80;
    else if (scheme.toLower() == "https")
        return 443;
    else if (scheme.toLower() == "rtsp")
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
    // TODO: #ak Introduce proper implementation.

    if (path.indexOf("//") == -1)
        return path;

    QString normalizedPath = path;
    normalizedPath.replace("//", "/");
    return normalizedPath;
}

} // namespace url
} // namespace network
} // namespace nx
