#pragma once

#include <string>

#include <nx/utils/basic_factory.h>

#include "abstract_transaction_transport_connector.h"
#include "../compatible_ec2_protocol_version.h"

namespace nx::clusterdb::engine {

class CommandLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport {

using ConnectorFactoryFunc = std::unique_ptr<AbstractTransactionTransportConnector>(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const nx::utils::Url& nodeUrl,
    const std::string& systemId,
    const std::string& nodeId);

class NX_DATA_SYNC_ENGINE_API ConnectorFactory:
    public nx::utils::BasicFactory<ConnectorFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<ConnectorFactoryFunc>;

public:
    ConnectorFactory();

    static ConnectorFactory& instance();

private:
    std::unique_ptr<AbstractTransactionTransportConnector> defaultFactoryFunction(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const nx::utils::Url& nodeUrl,
        const std::string& systemId,
        const std::string& nodeId);
};

} // namespace nx::clusterdb::engine::transport
