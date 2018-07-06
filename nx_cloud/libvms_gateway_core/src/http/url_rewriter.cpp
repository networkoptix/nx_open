#include "url_rewriter.h"

#include <nx/network/url/url_parse_helper.h>

#include "http_api_path.h"

namespace nx {
namespace cloud {
namespace gateway {

nx::utils::Url UrlRewriter::originalResourceUrlToProxyUrl(
    const nx::utils::Url& originalResourceUrl,
    const network::SocketAddress& proxyEndpoint,
    const nx::String& targetHost) const
{
    nx::utils::Url url = originalResourceUrl;

    url.setPath(nx::network::url::normalizePath(
        lm("/%1/%2/%3").arg(kApiPathPrefix).arg(targetHost).arg(url.path())));
    if (!url.host().isEmpty())
    {
        url.setHost(proxyEndpoint.address.toString());
        if (proxyEndpoint.port != 0)
            url.setPort(proxyEndpoint.port);
    }

    return url;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
