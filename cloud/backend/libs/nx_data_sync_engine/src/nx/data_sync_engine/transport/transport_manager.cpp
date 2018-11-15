#include "transport_manager.h"

#include "http_tunnel_transport_connector.h"

namespace nx::clusterdb::engine::transport {

std::unique_ptr<AbstractTransactionTransportConnector> TransportManager::createConnector(
    const std::string& systemId,
    const std::string& connectionId,
    const nx::utils::Url& targetUrl)
{
    return std::make_unique<HttpTunnelTransportConnector>(
        systemId,
        connectionId,
        targetUrl);
}

} // namespace nx::clusterdb::engine::transport
