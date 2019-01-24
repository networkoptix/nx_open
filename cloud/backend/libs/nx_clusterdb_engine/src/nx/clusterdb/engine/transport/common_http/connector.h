#pragma once

#include <nx/utils/url.h>

#include <transaction/transaction_transport_base.h>

#include "../abstract_transaction_transport_connector.h"
#include "../../compatible_ec2_protocol_version.h"

namespace nx::clusterdb::engine {

class CommandLog;
class OutgoingCommandFilter;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport {

class NX_DATA_SYNC_ENGINE_API HttpCommandPipelineConnector:
    public AbstractCommandPipelineConnector
{
    using base_type = AbstractCommandPipelineConnector;

public:
    HttpCommandPipelineConnector(
        const ProtocolVersionRange& protocolVersionRange,
        const nx::utils::Url& nodeUrl,
        const std::string& systemId,
        const std::string& nodeId);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(Handler completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange m_protocolVersionRange;
    const nx::utils::Url m_getCommandsNodeUrl;
    const nx::utils::Url m_postCommandsNodeUrl;
    const std::string m_systemId;
    nx::vms::api::PeerData m_peerData;

    Handler m_completionHandler;
    std::unique_ptr<::ec2::QnTransactionTransportBase> m_connection;
    std::shared_ptr<::ec2::ConnectionGuardSharedState> m_connectionGuardSharedState;
    QMetaObject::Connection m_stateChangedConnection;

    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);
    void processSuccessfulConnect();
    void processConnectFailure();
};

//-------------------------------------------------------------------------------------------------

class NX_DATA_SYNC_ENGINE_API HttpTransportConnector:
    public AbstractTransactionTransportConnector
{
    using base_type = AbstractTransactionTransportConnector;

public:
    HttpTransportConnector(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* transactionLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const nx::utils::Url& nodeUrl,
        const std::string& systemId,
        const std::string& nodeId);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(Handler completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const std::string m_systemId;
    HttpCommandPipelineConnector m_pipelineConnector;
    Handler m_completionHandler;

    void onPipelineConnectCompleted(
        ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<AbstractCommandPipeline> connection);
};

} // namespace nx::clusterdb::engine::transport {
