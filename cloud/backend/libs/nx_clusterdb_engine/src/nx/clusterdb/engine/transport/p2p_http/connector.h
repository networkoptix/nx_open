#pragma once

#include <string>

#include <nx/p2p/transport/p2p_http_client_transport.h>
#include <nx/vms/api/data/peer_data.h>

#include "../abstract_transaction_transport_connector.h"

namespace nx::clusterdb::engine {

class ProtocolVersionRange;
class CommandLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport::p2p::http {

class NX_DATA_SYNC_ENGINE_API Connector:
    public AbstractTransactionTransportConnector
{
    using base_type = AbstractTransactionTransportConnector;

public:
    Connector(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& filter,
        const nx::utils::Url& remoteNodeUrl,
        const std::string& systemId,
        const std::string& nodeId);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(Handler completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange& m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    const OutgoingCommandFilter& m_commandFilter;
    nx::utils::Url m_remoteNodeUrl;
    const std::string m_systemId;

    std::string m_connectionId;
    std::unique_ptr<nx::p2p::P2PHttpClientTransport> m_commandPipeline;
    Handler m_completionHandler;
    nx::vms::api::PeerDataEx m_localPeer;

    void handlePipelineStart(SystemError::ErrorCode systemErrorCode);
};

} // namespace nx::clusterdb::engine::transport::p2p::http
