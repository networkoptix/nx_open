#include "factory.h"

#include "acceptor.h"
#include "connector.h"

namespace nx::clusterdb::engine::transport::http_tunnel {

std::unique_ptr<AbstractTransactionTransportConnector> Factory::createConnector(
    const ProtocolVersionRange& /*protocolVersionRange*/,
    CommandLog* /*commandLog*/,
    const OutgoingCommandFilter& /*outgoingCommandFilter*/,
    const std::string& clusterId,
    const std::string& /*nodeId*/,
    const nx::utils::Url& targetUrl)
{
    return std::make_unique<HttpTunnelTransportConnector>(
        clusterId,
        QnUuid::createUuid().toSimpleString().toStdString(), // TODO
        targetUrl);
}

std::unique_ptr<AbstractAcceptor> Factory::createAcceptor(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    ConnectionManager* connectionManager,
    const QnUuid& nodeId)
{
    return std::make_unique<HttpTunnelTransportAcceptor>(
        nodeId,
        protocolVersionRange,
        commandLog,
        connectionManager,
        outgoingCommandFilter);
}

} // namespace nx::clusterdb::engine::transport::http_tunnel
