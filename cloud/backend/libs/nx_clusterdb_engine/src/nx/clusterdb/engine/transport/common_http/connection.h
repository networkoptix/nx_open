#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <nx/vms/api/data/tran_state_data.h>
#include <transaction/transaction_transport_base.h>

#include "../abstract_transaction_transport.h"
#include "../../compatible_ec2_protocol_version.h"

namespace ec2 {
class QnTransactionTransportBase;
class ConnectionGuardSharedState;
} // namespace ec2

namespace nx::clusterdb::engine { class CommandLog; }

namespace nx::clusterdb::engine::transport {

class CommonHttpConnection:
    public QObject,
    public AbstractCommandPipeline
{
    using base_type = AbstractCommandPipeline;

public:
    /**
     * Initializer for incoming connection.
     */
    CommonHttpConnection(
        const ProtocolVersionRange& protocolVersionRange,
        nx::network::aio::AbstractAioThread* aioThread,
        std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
        const ConnectionRequestAttributes& connectionRequestAttributes,
        const std::string& systemId,
        const vms::api::PeerData& localPeer,
        const network::SocketAddress& remotePeerEndpoint,
        const nx::network::http::Request& request);

    CommonHttpConnection(
        const ProtocolVersionRange& protocolVersionRange,
        std::unique_ptr<::ec2::QnTransactionTransportBase> connection,
        std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
        const std::string& systemId,
        const network::SocketAddress& remotePeerEndpoint);

    virtual ~CommonHttpConnection();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual void start() override;
    virtual network::SocketAddress remotePeerEndpoint() const override;
    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;
    virtual void setOnGotTransaction(CommandDataHandler handler) override;
    virtual std::string connectionGuid() const override;
    virtual void sendTransaction(
        CommandTransportHeader transportHeader,
        const std::shared_ptr<const CommandSerializer>& transactionSerializer) override;
    virtual void closeConnection() override;

    void receivedTransaction(
        const nx::network::http::HttpHeaders& headers,
        const QnByteArrayConstRef& tranData);

    void setOutgoingConnection(std::unique_ptr<network::AbstractCommunicatingSocket> socket);

    nx::vms::api::PeerData localPeer() const;
    nx::vms::api::PeerData remotePeer() const;
    int remotePeerProtocolVersion() const;

private:
    const ProtocolVersionRange m_protocolVersionRange;
    std::shared_ptr<::ec2::ConnectionGuardSharedState> m_connectionGuardSharedState;
    std::unique_ptr<::ec2::QnTransactionTransportBase> m_baseTransactionTransport;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    CommandDataHandler m_gotTransactionEventHandler;
    const std::string m_systemId;
    const std::string m_connectionId;
    const network::SocketAddress m_connectionOriginatorEndpoint;
    bool m_closed = false;
    std::unique_ptr<network::aio::Timer> m_inactivityTimer;
    std::optional<int> m_prevReceivedTransportSequence;

    int highestProtocolVersionCompatibleWithRemotePeer() const;

    void onGotTransaction(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        VmsTransportHeader transportHeader);

    void forwardTransactionToProcessor(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        VmsTransportHeader transportHeader);

    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);

    void forwardStateChangedEvent(
        ::ec2::QnTransactionTransportBase::State newState);

    void restartInactivityTimer();
    void onInactivityTimeout();
};

} // namespace nx::clusterdb::engine::transport
