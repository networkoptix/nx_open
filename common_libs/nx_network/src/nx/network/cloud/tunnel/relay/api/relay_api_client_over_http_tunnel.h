#pragma once

#include <list>

#include <nx/network/http/tunneling/client.h>

#include "relay_api_basic_client.h"

namespace nx::cloud::relay::api {

class NX_NETWORK_API ClientOverHttpTunnel:
    public BasicClient
{
    using base_type = BasicClient;

public:
    ClientOverHttpTunnel(
        const QUrl& baseUrl,
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
        std::list<std::unique_ptr<nx_http::tunneling::Client>>;

    using OpenTrafficRelayTunnelHandler = nx::utils::MoveOnlyFunc<void(
        const nx_http::tunneling::Client&,
        nx_http::tunneling::OpenTunnelResult)>;

    TunnelingClients m_tunnelingClients;

    void openServerTunnel(
        const std::string& peerName,
        BeginListeningHandler completionHandler);

    void openTunnel(
        const QUrl& url,
        nx_http::tunneling::TunnelValidatorFactoryFunc tunnelValidatorFactoryFunc,
        ClientOverHttpTunnel::OpenTrafficRelayTunnelHandler handler);

    void processServerTunnelResult(
        BeginListeningHandler completionHandler,
        const nx_http::tunneling::Client& tunnelingClient,
        nx_http::tunneling::OpenTunnelResult result);

    void openClientTunnel(
        const std::string& sessionId,
        OpenRelayConnectionHandler completionHandler);

    void processClientTunnelResult(
        OpenRelayConnectionHandler completionHandler,
        const nx_http::tunneling::Client& tunnelingClient,
        nx_http::tunneling::OpenTunnelResult result);

    static api::ResultCode getResultCode(
        const nx_http::tunneling::OpenTunnelResult& tunnelResult,
        const nx_http::tunneling::Client& tunnelingClient);
};

} // namespace nx::cloud::relay::api
