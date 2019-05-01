#pragma once

#include "../abstract_transport_factory.h"

namespace nx::clusterdb::engine::transport::http_tunnel {

class Factory:
    public AbstractFactory
{
public:
    virtual std::unique_ptr<AbstractTransactionTransportConnector> createConnector(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const std::string& clusterId,
        const std::string& nodeId,
        const nx::utils::Url& targetUrl) override;

    virtual std::unique_ptr<AbstractAcceptor> createAcceptor(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        ConnectionManager* connectionManager,
        const QnUuid& nodeId) override;
};

} // namespace nx::clusterdb::engine::transport::http_tunnel
