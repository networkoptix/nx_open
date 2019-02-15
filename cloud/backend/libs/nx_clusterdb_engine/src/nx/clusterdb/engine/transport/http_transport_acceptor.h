#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/thread/mutex.h>

#include <nx/vms/api/data/peer_data.h>

#include <transaction/connection_guard_shared_state.h>

#include "../transaction_transport.h"
#include "command_transport_delegate.h"

namespace nx::clusterdb::engine {

class ConnectionManager;
class ProtocolVersionRange;
class OutgoingCommandFilter;
class CommandLog;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport {

class GenericTransport;

class CommonHttpAcceptor
{
public:
    CommonHttpAcceptor(
        const QnUuid& peerId,
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* transactionLog,
        ConnectionManager* connectionManager,
        const OutgoingCommandFilter& outgoingCommandFilter);

    /**
     * Mediaserver calls this method to open 2-way transaction exchange channel.
     */
    void createConnection(
        nx::network::http::RequestContext requestContext,
        const std::string& systemId,
        nx::network::http::RequestProcessedHandler completionHandler);

    void pushTransaction(
        nx::network::http::RequestContext requestContext,
        const std::string& systemId,
        nx::network::http::RequestProcessedHandler completionHandler);

private:
    const ProtocolVersionRange& m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    ConnectionManager* m_connectionManager = nullptr;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const vms::api::PeerData m_localPeerData;
    std::shared_ptr<::ec2::ConnectionGuardSharedState> m_connectionGuardSharedState;
    std::atomic<int> m_connectionSeq{0};

    nx::network::http::RequestResult prepareOkResponseToCreateTransactionConnection(
        const ConnectionRequestAttributes& connectionRequestAttributes,
        const vms::api::PeerData& localPeerData,
        nx::network::http::Response* const response);

    void startOutgoingChannel(
        const std::string& connectionId,
        int connectionSeq,
        CommonHttpConnection* commandPipeline,
        nx::network::http::HttpServerConnection* httpConnection);

    void postTransactionToTransport(
        AbstractConnection* transportConnection,
        nx::network::http::Request request,
        bool* foundConnectionOfExpectedType);
};

} // namespace nx::clusterdb::engine::transport
