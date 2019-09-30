#include "url_parse_helper.h"

#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/utils/string.h>

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
    std::vector<std::string_view> tokens;
    tokens.reserve(std::count(path.begin(), path.end(), '/') + 1);

    if (nx::utils::startsWith(path, '/'))
        tokens.push_back(std::string_view());

    nx::utils::split(
        path, '/',
        [&tokens, tokenCountBak = tokens.size()](
            const std::string_view& token)
        {
            if (token == ".")
                return;

            if (token == ".." && tokens.size() > tokenCountBak)
            {
                tokens.pop_back();
                return;
            }

            tokens.push_back(token);
        });

    if (nx::utils::endsWith(path, '/'))
        tokens.push_back(std::string_view());

    return nx::utils::join(tokens.begin(), tokens.end(), '/');
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
