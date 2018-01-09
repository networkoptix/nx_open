#pragma once

#include <atomic>
#include <memory>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/public_ip_discovery.h>
#include <nx/utils/service.h>

#include <nx/cloud/relaying/relay_engine.h>

#include "settings.h"
#include "run_time_options.h"

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
    ~VmsGatewayProcess();

    const std::vector<network::SocketAddress>& httpEndpoints() const;

    relaying::RelayEngine& relayEngine();
    const relaying::RelayEngine& relayEngine() const;

    void enforceSslFor(const network::SocketAddress& targetAddress, bool enabled = true);

protected:
    virtual std::unique_ptr<nx::utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const nx::utils::AbstractServiceSettings& settings) override;

private:
    conf::RunTimeOptions m_runTimeOptions;
    std::vector<network::SocketAddress> m_httpEndpoints;
    nx::network::cloud::tcp::EndpointVerificatorFactory::Function m_endpointVerificatorFactoryBak;
    relaying::RelayEngine* m_relayEngine = nullptr;

    void initializeCloudConnect(const conf::Settings& settings);

    std::unique_ptr<nx::network::PublicIPDiscovery> initializePublicIpDiscovery(
        const conf::Settings& settings);
    void publicAddressFetched(
        const conf::Settings& settings,
        const QString& publicAddress);

    void registerApiHandlers(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions,
        relaying::RelayEngine* relayEngine,
        nx::network::http::server::rest::MessageDispatcher* const msgDispatcher);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
