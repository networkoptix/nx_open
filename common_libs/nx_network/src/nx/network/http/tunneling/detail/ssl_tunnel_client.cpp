#include "ssl_tunnel_client.h"

namespace nx_http::tunneling::detail {

SslTunnelClient::SslTunnelClient(
    const QUrl& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(
        convertToHttpsUrl(baseTunnelUrl),
        std::move(clientFeedbackFunction))
{
}

QUrl SslTunnelClient::convertToHttpsUrl(
    QUrl httpUrl)
{
    if (httpUrl.scheme() == kSecureUrlSchemeName)
        return httpUrl;

    httpUrl.setScheme(kSecureUrlSchemeName);
    if (httpUrl.port() == defaultPortForScheme(kUrlSchemeName))
        httpUrl.setPort(defaultPortForScheme(kSecureUrlSchemeName));

    return httpUrl;
}

} // namespace nx_http::tunneling::detail
