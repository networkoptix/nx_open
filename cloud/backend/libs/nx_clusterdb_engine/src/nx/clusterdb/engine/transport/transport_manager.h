#pragma once

#include <memory>
#include <string>

#include <nx/utils/url.h>

#include "abstract_transaction_transport_connector.h"
#include "../compatible_ec2_protocol_version.h"

namespace nx::clusterdb::engine {

class TransactionLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport {

class TransportManager
{
public:
    TransportManager(
        const ProtocolVersionRange& protocolVersionRange,
        TransactionLog* transactionLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const std::string& nodeId);

    std::unique_ptr<AbstractTransactionTransportConnector> createConnector(
        const std::string& systemId,
        const std::string& connectionId,
        const nx::utils::Url& targetUrl);

private:
    const ProtocolVersionRange m_protocolVersionRange;
    TransactionLog* m_transactionLog = nullptr;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const std::string m_nodeId;
};

} // namespace nx::clusterdb::engine::transport
