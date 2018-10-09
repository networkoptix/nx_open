#include "transport_manager.h"

#include "http_tunnel_transport_connector.h"

namespace nx::data_sync_engine {

std::unique_ptr<AbstractTransactionTransportConnector> TransportManager::createConnector(
    const std::string& systemId,
    const std::string& connectionId,
    const nx::utils::Url& targetUrl)
{
    return std::make_unique<transport::HttpTunnelTransportConnector>(
        systemId,
        connectionId,
        targetUrl);
}

} // namespace nx::data_sync_engine
