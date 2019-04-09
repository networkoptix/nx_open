#pragma once

#include <string>

#include <nx/network/http/http_async_client.h>

#include <nx/p2p/transport/p2p_websocket_transport.h>
#include <nx/vms/api/data/peer_data.h>

#include "../abstract_transaction_transport_connector.h"

namespace nx::clusterdb::engine {

class ProtocolVersionRange;
class CommandLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport::p2p::websocket {

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
    int m_stage2TryCount = 0;

    std::unique_ptr<network::http::AsyncClient> m_connectionUpgradeClient;
    std::unique_ptr<nx::p2p::P2PWebsocketTransport> m_commandPipeline;
    std::string m_connectionId;
    Handler m_completionHandler;
    nx::vms::api::PeerDataEx m_localPeer;
    nx::vms::api::PeerDataEx m_remotePeerData;

    void handleUpgradeCompletion();
    void upgradeHttpConnectionToCommandTransportConnection();
    void sendConnectStage2Request();
    void handlePipelineStart(SystemError::ErrorCode resultCode);
};

} // namespace nx::clusterdb::engine::transport::p2p::websocket
