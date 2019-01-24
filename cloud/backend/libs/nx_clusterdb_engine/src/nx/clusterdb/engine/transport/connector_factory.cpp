#include "connector_factory.h"

#include "common_http/http_transport_connector.h"
#include "http_tunnel/http_tunnel_transport_connector.h"

namespace nx::clusterdb::engine::transport {

ConnectorFactory::ConnectorFactory():
    base_type([this](auto&&... args) { return defaultFactoryFunction(std::move(args)...); })
{
}

ConnectorFactory& ConnectorFactory::instance()
{
    static ConnectorFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractTransactionTransportConnector> ConnectorFactory::defaultFactoryFunction(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const nx::utils::Url& nodeUrl,
    const std::string& systemId,
    const std::string& nodeId)
{
#if 0
    return std::make_unique<HttpTunnelTransportConnector>(
        systemId,
        connectionId,
        syncApiTargetUrl);
#else
    return std::make_unique<HttpTransportConnector>(
        protocolVersionRange,
        commandLog,
        outgoingCommandFilter,
        nodeUrl,
        systemId,
        nodeId);
#endif
}

} // namespace nx::clusterdb::engine::transport
