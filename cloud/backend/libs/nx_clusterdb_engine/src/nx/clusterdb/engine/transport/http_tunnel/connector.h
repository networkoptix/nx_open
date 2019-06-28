#pragma once

#include <string>

#include <nx/network/http/tunneling/client.h>
#include <nx/utils/url.h>

#include <nx/vms/api/data/peer_data.h>

#include "../abstract_command_transport_connector.h"

namespace nx::clusterdb::engine {

class ProtocolVersionRange;
class CommandLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport::http_tunnel {

class Connector:
    public AbstractTransactionTransportConnector
{
    using base_type = AbstractTransactionTransportConnector;

public:
    Connector(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const std::string& clusterId,
        const std::string& nodeId,
        const nx::utils::Url& targetUrl);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(Handler completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange& m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const std::string m_clusterId;
    const std::string m_nodeId;
    const nx::utils::Url m_targetUrl;
    std::string m_connectionId;
    std::unique_ptr<nx::network::http::tunneling::Client> m_client;
    Handler m_completionHandler;
    nx::vms::api::PeerDataEx m_localPeer;

    void processOpenTunnelResult(
        nx::network::http::tunneling::OpenTunnelResult result);
};

} // namespace nx::clusterdb::engine::transport::http_tunnel
