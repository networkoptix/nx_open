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
    else if (scheme.toLower() == nx::network::rtsp::kUrlSchemeName)
        return 554;

    return 0;
}

SocketAddress getEndpoint(const nx::utils::Url& url)
{
    return SocketAddress(
        url.host(),
        static_cast<quint16>(url.port(getDefaultPortForScheme(url.scheme()))));
}

std::string normalizePath(const std::string& path)
{
    if (path.find("//") == std::string::npos)
        return path;

    std::string normalizedPath;
    normalizedPath.reserve(path.size());

    char prevCh = ' '; //< Anything but not /.
    for (const auto& ch: path)
    {
        if (ch == '/' && prevCh == '/')
            continue;
        prevCh = ch;
        normalizedPath.push_back(ch);
    }

    return normalizedPath;
}

QString normalizePath(const QString& path)
{
    return QString::fromStdString(normalizePath(path.toStdString()));
}

std::string normalizePath(const char* path)
{
    return normalizePath(std::string(path));
}

namespace detail {

std::string joinPath(const std::string& left, const std::string& right)
{
    return normalizePath(left + "/" + right);
}

} // namespace detail

} // namespace url
} // namespace network
} // namespace nx
