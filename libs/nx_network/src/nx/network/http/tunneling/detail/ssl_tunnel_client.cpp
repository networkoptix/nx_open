#include "ssl_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

SslTunnelClient::SslTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(
        convertToHttpsUrl(baseTunnelUrl),
        std::move(clientFeedbackFunction))
{
}

nx::utils::Url SslTunnelClient::convertToHttpsUrl(
    nx::utils::Url httpUrl)
{
    if (httpUrl.scheme() == kSecureUrlSchemeName)
        return httpUrl;

    httpUrl.setScheme(kSecureUrlSchemeName);
    if (httpUrl.port() == defaultPortForScheme(kUrlSchemeName))
        httpUrl.setPort(defaultPortForScheme(kSecureUrlSchemeName));

    return httpUrl;
}

} // namespace nx::network::http::tunneling::detail
