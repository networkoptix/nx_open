#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nx/utils/url.h>

#include "abstract_command_transport_connector.h"
#include "abstract_transport_factory.h"
#include "../compatible_ec2_protocol_version.h"

namespace nx::clusterdb::engine {

class CommandLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport {

class NX_DATA_SYNC_ENGINE_API TransportManager
{
public:
    TransportManager(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* transactionLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        ConnectionManager* connectionManager,
        const std::string& nodeId);

    TransportManager(const TransportManager&) = delete;

    void registerTransport(std::unique_ptr<AbstractFactory> transportFactory);

    // TODO: #ak Give up nodeId argument.
    std::vector<std::unique_ptr<AbstractAcceptor>> createAllAcceptors(
        const QnUuid& nodeId);

    /**
     * Transport factory with AbstractFactory::key() equal to key will be used to create
     * AbstractTransactionTransportConnector.
     * Otherwise, the first registered type is used.
     * @return true if the requested type was found.
     */
    bool setConnectorTypeKey(const std::string& key);

    std::unique_ptr<AbstractTransactionTransportConnector> createConnector(
        const std::string& clusterId,
        const std::string& connectionId,
        const nx::utils::Url& targetUrl);

private:
    const ProtocolVersionRange m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    ConnectionManager* m_connectionManager = nullptr;
    const std::string m_nodeId;
    std::vector<std::unique_ptr<AbstractFactory>> m_transportFactories;
    AbstractFactory* m_connectorFactory = nullptr;
};

} // namespace nx::clusterdb::engine::transport
