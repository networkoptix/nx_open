#pragma once

#include <memory>
#include <string>

#include <nx/utils/uuid.h>

#include "abstract_acceptor.h"
#include "abstract_command_transport_connector.h"
#include "../connection_manager.h"

namespace nx::clusterdb::engine {

class CommandLog;
class ConnectionManager;
class OutgoingCommandFilter;
class ProtocolVersionRange;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport {

/**
 * Constructs transport-related types.
 */
class AbstractFactory
{
public:
    virtual ~AbstractFactory() = default;

    virtual std::string key() const = 0;

    virtual std::unique_ptr<AbstractTransactionTransportConnector> createConnector(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const std::string& clusterId,
        const std::string& nodeId,
        const nx::utils::Url& targetUrl) = 0;

    virtual std::unique_ptr<AbstractAcceptor> createAcceptor(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        ConnectionManager* connectionManager,
        const QnUuid& nodeId) = 0;
};

} // namespace nx::clusterdb::engine::transport
