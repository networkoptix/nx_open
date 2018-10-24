#pragma once

#include <memory>
#include <vector>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <nx/vms/api/data/tran_state_data.h>

#include <transaction/transaction_transport_base.h>

#include "abstract_transaction_transport.h"
#include "serialization/transaction_serializer.h"
#include "transaction_processor.h"
#include "transaction_transport_header.h"

namespace ec2 {
class QnTransactionTransportBase;
class ConnectionGuardSharedState;
} // namespace ec2

namespace nx {
namespace data_sync_engine {

class TransactionLog;

class TransactionTransport:
    public QObject,
    public AbstractCommandPipeline
{
    using base_type = AbstractCommandPipeline;

public:
    /**
     * Initializer for incoming connection.
     */
    TransactionTransport(
        const ProtocolVersionRange& protocolVersionRange,
        nx::network::aio::AbstractAioThread* aioThread,
        std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
        const transport::ConnectionRequestAttributes& connectionRequestAttributes,
        const std::string& systemId,
        const vms::api::PeerData& localPeer,
        const network::SocketAddress& remotePeerEndpoint,
        const nx::network::http::Request& request);
    virtual ~TransactionTransport();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual network::SocketAddress remotePeerEndpoint() const override;
    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;
    virtual void setOnGotTransaction(GotTransactionEventHandler handler) override;
    virtual QnUuid connectionGuid() const override;
    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const TransactionSerializer>& transactionSerializer) override;
    virtual void closeConnection() override;

    void receivedTransaction(
        const nx::network::http::HttpHeaders& headers,
        const QnByteArrayConstRef& tranData);

    void setOutgoingConnection(std::unique_ptr<network::AbstractCommunicatingSocket> socket);

private:
    const ProtocolVersionRange m_protocolVersionRange;
    std::shared_ptr<::ec2::ConnectionGuardSharedState> m_connectionGuardSharedState;
    std::unique_ptr<::ec2::QnTransactionTransportBase> m_baseTransactionTransport;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    const std::string m_systemId;
    const std::string m_connectionId;
    const network::SocketAddress m_connectionOriginatorEndpoint;
    bool m_closed = false;
    std::unique_ptr<network::aio::Timer> m_inactivityTimer;

    int highestProtocolVersionCompatibleWithRemotePeer() const;

    void onGotTransaction(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        CommandTransportHeader transportHeader);

    void forwardTransactionToProcessor(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        CommandTransportHeader transportHeader);

    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);

    void forwardStateChangedEvent(
        ::ec2::QnTransactionTransportBase::State newState);

    void restartInactivityTimer();
    void onInactivityTimeout();
};

} // namespace data_sync_engine
} // namespace nx
