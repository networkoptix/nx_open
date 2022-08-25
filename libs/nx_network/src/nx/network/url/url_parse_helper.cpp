// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "url_parse_helper.h"

#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/utils/string.h>

namespace nx::network::url {

std::uint16_t getDefaultPortForScheme(const std::string_view& scheme)
{
    if (nx::utils::stricmp(scheme, nx::network::http::kUrlSchemeName) == 0)
        return 80;
    else if (nx::utils::stricmp(scheme, nx::network::http::kSecureUrlSchemeName) == 0)
        return 443;
    else if (nx::utils::stricmp(scheme, nx::network::rtsp::kUrlSchemeName) == 0)
        return 554;

    return 0;
}

SocketAddress getEndpoint(const nx::utils::Url& url, int defaultPort)
{
    return SocketAddress(
        url.host().toStdString(),
        static_cast<std::uint16_t>(url.port(defaultPort)));
}

SocketAddress getEndpoint(const nx::utils::Url& url)
{
    return getEndpoint(url, getDefaultPortForScheme(url.scheme().toStdString()));
}

std::string normalizePath(const std::string_view& path)
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

QString normalizedPath(const QString& path, const QString& pathIgnorePrefix)
{
    int startIndex = 0;
    while (startIndex < path.length() && path[startIndex] == '/')
        ++startIndex;

    int endIndex = path.size(); // [startIndex..endIndex)
    while (endIndex > 0 && path[endIndex-1] == '/')
        --endIndex;

    if (path.mid(startIndex).startsWith(pathIgnorePrefix))
        startIndex += pathIgnorePrefix.length();
    return path.mid(startIndex, endIndex - startIndex);
}

namespace detail {

std::string joinPath(const std::string_view& left, const std::string_view& right)
{
    return normalizePath(nx::utils::buildString(left, "/", right));
}

} // namespace detail

} // namespace nx::network::url
