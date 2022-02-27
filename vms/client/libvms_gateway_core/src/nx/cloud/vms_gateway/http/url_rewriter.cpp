// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "url_rewriter.h"

#include <nx/network/url/url_parse_helper.h>

#include "http_api_path.h"

namespace nx {
namespace cloud {
namespace gateway {

nx::utils::Url UrlRewriter::originalResourceUrlToProxyUrl(
    const nx::utils::Url& originalResourceUrl,
    const nx::utils::Url& proxyHostUrl,
    const std::string& targetHost) const
{
    nx::utils::Url url = originalResourceUrl;

    url.setPath(nx::network::url::normalizePath(
        nx::format("/%1/%2/%3").arg(kApiPathPrefix).arg(targetHost).arg(url.path()).toStdString()));
    if (!url.host().isEmpty())
    {
        url.setHost(proxyHostUrl.host());
        if (proxyHostUrl.port() > 0)
            url.setPort(proxyHostUrl.port());
    }

    return url;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
