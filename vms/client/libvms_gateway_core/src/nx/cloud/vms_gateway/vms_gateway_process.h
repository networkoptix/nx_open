// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/public_ip_discovery.h>
#include <nx/utils/service.h>

#include <nx/network/aio/async_channel_bridge.h>

#include "http/channel_bridge_to_server_connection_adaptor.h"
#include "run_time_options.h"
#include "settings.h"

namespace nx {
namespace cloud {

namespace relaying { class RelayEngine; }

namespace gateway {

class AuthorizationManager;

class NX_VMS_GATEWAY_API VmsGatewayProcess:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    VmsGatewayProcess(int argc, char **argv);
    ~VmsGatewayProcess() override;

    const std::vector<network::SocketAddress>& httpEndpoints() const;

    void enforceSslFor(const network::SocketAddress& targetAddress, bool enabled = true);
    void enforceHeadersFor(const network::SocketAddress& targetAddress, network::http::HttpHeaders headers);

protected:
    virtual std::unique_ptr<nx::utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const nx::utils::AbstractServiceSettings& settings) override;

private:
    using HttpConnectTunnelPool =
        network::server::StreamServerConnectionHolder<BridgeToServerConnectionAdaptor>;

    conf::RunTimeOptions m_runTimeOptions;
    std::vector<network::SocketAddress> m_httpEndpoints;
    nx::network::cloud::tcp::EndpointVerificatorFactory::Function m_endpointVerificatorFactoryBak;

    void initializeCloudConnect(const conf::Settings& settings);

    std::unique_ptr<nx::network::PublicIPDiscovery> initializePublicIpDiscovery(
        const conf::Settings& settings);

    void publicAddressFetched(const QString& publicAddress);

    void registerApiHandlers(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions,
        nx::network::http::server::rest::MessageDispatcher* const msgDispatcher,
        HttpConnectTunnelPool* httpConnectTunnelPool);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
