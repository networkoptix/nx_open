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
#include "transaction_log_reader.h"
#include "transaction_transport_header.h"

namespace ec2 {
class QnTransactionTransportBase;
class ConnectionGuardSharedState;
} // namespace ec2

namespace nx {
namespace data_sync_engine {

class TransactionLog;

struct ConnectionRequestAttributes
{
    std::string connectionId;
    vms::api::PeerData remotePeer;
    std::string contentEncoding;
    int remotePeerProtocolVersion = 0;
};

class TransactionTransport:
    public QObject,
    public AbstractTransactionTransport
{
    using base_type = AbstractTransactionTransport;

public:
    /**
     * Initializer for incoming connection.
     */
    TransactionTransport(
        const ProtocolVersionRange& protocolVersionRange,
        nx::network::aio::AbstractAioThread* aioThread,
        std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
        TransactionLog* const transactionLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const ConnectionRequestAttributes& connectionRequestAttributes,
        const std::string& systemId,
        const vms::api::PeerData& localPeer,
        const network::SocketAddress& remotePeerEndpoint,
        const nx::network::http::Request& request);
    virtual ~TransactionTransport();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual network::SocketAddress remoteSocketAddr() const override;
    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;
    virtual void setOnGotTransaction(GotTransactionEventHandler handler) override;
    virtual QnUuid connectionGuid() const override;
    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;
    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

    void receivedTransaction(
        const nx::network::http::HttpHeaders& headers,
        const QnByteArrayConstRef& tranData);

    void setOutgoingConnection(std::unique_ptr<network::AbstractCommunicatingSocket> socket);

    void startOutgoingChannel();

    void processSpecialTransaction(
        const TransactionTransportHeader& transportHeader,
        Command<vms::api::SyncRequestData> data,
        TransactionProcessedHandler handler);

    void processSpecialTransaction(
        const TransactionTransportHeader& transportHeader,
        Command<vms::api::TranStateResponse> data,
        TransactionProcessedHandler handler);

    void processSpecialTransaction(
        const TransactionTransportHeader& transportHeader,
        Command<vms::api::TranSyncDoneData> data,
        TransactionProcessedHandler handler);

private:
    const ProtocolVersionRange m_protocolVersionRange;
    std::shared_ptr<::ec2::ConnectionGuardSharedState> m_connectionGuardSharedState;
    std::unique_ptr<::ec2::QnTransactionTransportBase> m_baseTransactionTransport;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    std::unique_ptr<TransactionLogReader> m_transactionLogReader;
    const std::string m_systemId;
    const std::string m_connectionId;
    const network::SocketAddress m_connectionOriginatorEndpoint;
    TransactionTransportHeader m_commonTransportHeaderOfRemoteTransaction;
    /**
     * Transaction state, we need to synchronize remote side to, before we can mark it write sync.
     */
    vms::api::TranState m_tranStateToSynchronizeTo;
    /**
     * Transaction state of remote peer. Transactions before this state have been sent to the peer.
     */
    vms::api::TranState m_remotePeerTranState;
    bool m_haveToSendSyncDone;
    bool m_closed;
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

    void onTransactionsReadFromLog(
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransaction,
        vms::api::TranState readedUpTo);

    void enableOutputChannel();

    void restartInactivityTimer();
    void onInactivityTimeout();

    template<typename CommandDescriptor>
    void sendTransaction(
        Command<typename CommandDescriptor::Data> transaction,
        TransactionTransportHeader transportHeader);
};

} // namespace data_sync_engine
} // namespace nx
