// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    /**
     * @param forcedHttpTunnelType passed to the nx::network::http::tunneling::Client.
     */
    ClientOverHttpTunnel(
        const nx::utils::Url& baseUrl,
        std::optional<int> forcedHttpTunnelType);

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

    std::optional<int> m_forcedHttpTunnelType;
    TunnelingClients m_tunnelingClients;

    void openServerTunnel(
        const std::string& peerName,
        BeginListeningHandler completionHandler);

    void openTunnel(
        std::unique_ptr<network::http::tunneling::Client> client,
        network::http::tunneling::TunnelValidatorFactoryFunc tunnelValidatorFactoryFunc,
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

    void setPrevRequestFields(
        const network::http::tunneling::OpenTunnelResult& tunnelResult);

    static api::ResultCode getResultCode(
        const network::http::tunneling::OpenTunnelResult& tunnelResult,
        const network::http::tunneling::Client& tunnelingClient);
};

} // namespace nx::cloud::relay::api::detail
