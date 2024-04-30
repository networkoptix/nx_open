// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ssl_tunnel_client.h"

#include <nx/utils/log/log.h>

namespace nx::network::http::tunneling::detail {

SslTunnelClient::SslTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    const ConnectOptions& options,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(
        convertToHttpsUrl(baseTunnelUrl),
        options,
        std::move(clientFeedbackFunction))
{
    NX_VERBOSE(this, "Opening SSL tunnel to %1. https URL: %2",
        baseTunnelUrl, convertToHttpsUrl(baseTunnelUrl));
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
