#include "url_parse_helper.h"

namespace nx {
namespace network {
namespace url {

SocketAddress getEndpoint(const QUrl& url)
{
    return SocketAddress(url.host(), static_cast<quint16>(url.port(0)));
}

QString normalizePath(const QString& path)
{
    // TODO: #ak Introduce proper implementation.

    if (path.startsWith("//"))
    {
        QString normalizedPath = path;
        normalizedPath.remove(0, 1);
        return normalizedPath;
    }
    return path;
}

} // namespace url
} // namespace network
} // namespace nx
