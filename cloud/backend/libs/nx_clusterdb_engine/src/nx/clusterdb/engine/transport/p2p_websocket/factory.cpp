#include "factory.h"

#include "acceptor.h"
#include "connector.h"

namespace nx::clusterdb::engine::transport::p2p::websocket {

std::string Factory::key() const
{
    return kKey;
}

std::unique_ptr<AbstractTransactionTransportConnector> Factory::createConnector(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const std::string& clusterId,
    const std::string& nodeId,
    const nx::utils::Url& targetUrl)
{
    return std::make_unique<Connector>(
        protocolVersionRange,
        commandLog,
        outgoingCommandFilter,
        clusterId,
        nodeId,
        targetUrl);
}

std::unique_ptr<AbstractAcceptor> Factory::createAcceptor(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    ConnectionManager* connectionManager,
    const QnUuid& nodeId)
{
    return std::make_unique<Acceptor>(
        nodeId,
        protocolVersionRange,
        commandLog,
        connectionManager,
        outgoingCommandFilter);
}

} // namespace nx::clusterdb::engine::transport::p2p::websocket
