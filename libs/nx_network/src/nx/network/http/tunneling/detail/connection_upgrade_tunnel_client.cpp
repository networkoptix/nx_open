#include "connection_upgrade_tunnel_client.h"

#include <nx/network/url/url_builder.h>

#include "request_paths.h"
#include "../../rest/http_rest_client.h"

namespace nx::network::http::tunneling::detail {

ConnectionUpgradeTunnelClient::ConnectionUpgradeTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(baseTunnelUrl, std::move(clientFeedbackFunction))
{
}

void ConnectionUpgradeTunnelClient::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void ConnectionUpgradeTunnelClient::openTunnel(
    OpenTunnelCompletionHandler completionHandler)
{
    using namespace nx::network;

    m_tunnelUrl = url::Builder(m_baseTunnelUrl)
        .appendPath(kConnectionUpgradeTunnelPath);
    m_completionHandler = std::move(completionHandler);

    m_httpClient = std::make_unique<AsyncClient>();
    if (m_timeout)
    {
        m_httpClient->setMessageBodyReadTimeout(*m_timeout);
        m_httpClient->setResponseReadTimeout(*m_timeout);
    }
    m_httpClient->bindToAioThread(getAioThread());
    // TODO: HTTP method should be configurable.
    m_httpClient->doUpgrade(
        m_tunnelUrl,
        kConnectionUpgradeTunnelMethod,
        kConnectionUpgradeTunnelProtocol,
        std::bind(&ConnectionUpgradeTunnelClient::processOpenTunnelResponse, this));
}

const Response& ConnectionUpgradeTunnelClient::response() const
{
    return m_openTunnelResponse;
}

void ConnectionUpgradeTunnelClient::processOpenTunnelResponse()
{
    if (!m_httpClient->hasRequestSucceeded() || 
        m_httpClient->response()->statusLine.statusCode != StatusCode::switchingProtocols)
    {
        return cleanUpFailedTunnel();
    }

    m_connection = m_httpClient->takeSocket();
    m_openTunnelResponse = std::move(*m_httpClient->response());
    m_httpClient.reset();

    reportSuccess();
}

} // namespace nx::network::http::tunneling::detail
