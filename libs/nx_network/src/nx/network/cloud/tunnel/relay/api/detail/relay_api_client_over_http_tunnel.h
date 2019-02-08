#pragma once

#include <list>

#include <nx/network/http/tunneling/client.h>

#include "relay_api_basic_client.h"

namespace nx::cloud::relay::api::detail {

class NX_NETWORK_API ClientOverHttpTunnel:
    public BasicClient
{
    using base_type = BasicClient;

public:
    ClientOverHttpTunnel(
        const nx::utils::Url& baseUrl,
        ClientFeedbackFunction /*feedbackFunction*/);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void beginListening(
        const std::string& peerName,
        BeginListeningHandler completionHandler) override;

    virtual void openConnectionToTheTargetHost(
        const std::string& sessionId,
        OpenRelayConnectionHandler handler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    using TunnelingClients =
        std::list<std::unique_ptr<network::http::tunneling::Client>>;

    using OpenTrafficRelayTunnelHandler = nx::utils::MoveOnlyFunc<void(
        const network::http::tunneling::Client&,
        network::http::tunneling::OpenTunnelResult)>;

    TunnelingClients m_tunnelingClients;

    void openServerTunnel(
        const std::string& peerName,
        BeginListeningHandler completionHandler);

    void openTunnel(
        const nx::utils::Url& url,
        ClientOverHttpTunnel::OpenTrafficRelayTunnelHandler handler);

    void processServerTunnelResult(
        BeginListeningHandler completionHandler,
        const network::http::tunneling::Client& tunnelingClient,
        network::http::tunneling::OpenTunnelResult result);

    void openClientTunnel(
        const std::string& sessionId,
        OpenRelayConnectionHandler completionHandler);

    void processClientTunnelResult(
        OpenRelayConnectionHandler completionHandler,
        const network::http::tunneling::Client& tunnelingClient,
        network::http::tunneling::OpenTunnelResult result);

    static api::ResultCode getResultCode(
        const network::http::tunneling::OpenTunnelResult& tunnelResult,
        const network::http::tunneling::Client& tunnelingClient);
};

} // namespace nx::cloud::relay::api::detail
