#pragma once

#include <vector>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <common/common_globals.h>
#include <nx_ec/data/api_tran_state_data.h>
#include <nx_ec/ec_proto_version.h>
#include <transaction/transaction_transport_base.h>

#include "serialization/transaction_serializer.h"
#include "transaction_processor.h"
#include "transaction_log_reader.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

class TransactionTransport:
    public ::ec2::QnTransactionTransportBase
{
    typedef ::ec2::QnTransactionTransportBase ParentType;

public:
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> ConnectionClosedEventHandler;
    typedef nx::utils::MoveOnlyFunc<void(
        Qn::SerializationFormat,
        const QByteArray&,
        TransactionTransportHeader)> GotTransactionEventHandler;

    /**
     * Initializer for incoming connection.
     */
    TransactionTransport(
        nx::network::aio::AbstractAioThread* aioThread,
        ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
        TransactionLog* const transactionLog,
        const nx::String& systemId,
        const nx::String& connectionId,
        const ::ec2::ApiPeerData& localPeer,
        const ::ec2::ApiPeerData& remotePeer,
        const SocketAddress& remotePeerEndpoint,
        const nx_http::Request& request,
        const QByteArray& contentEncoding);
    virtual ~TransactionTransport();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    /** Set handler to be executed when tcp connection is closed. */
    void setOnConnectionClosed(ConnectionClosedEventHandler handler);
    void setOnGotTransaction(GotTransactionEventHandler handler);

    void startOutgoingChannel();

    void processSpecialTransaction(
        const TransactionTransportHeader& transportHeader,
        ::ec2::QnTransaction<::ec2::ApiSyncRequestData> data,
        TransactionProcessedHandler handler);

    void processSpecialTransaction(
        const TransactionTransportHeader& transportHeader,
        ::ec2::QnTransaction<::ec2::QnTranStateResponse> data,
        TransactionProcessedHandler handler);

    void processSpecialTransaction(
        const TransactionTransportHeader& transportHeader,
        ::ec2::QnTransaction<::ec2::ApiTranSyncDoneData> data,
        TransactionProcessedHandler handler);

    void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer);

    const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const;

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;
    virtual void onSomeDataReceivedFromRemotePeer() override;

private:
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    std::unique_ptr<TransactionLogReader> m_transactionLogReader;
    const nx::String m_systemId;
    const nx::String m_connectionId;
    const SocketAddress m_connectionOriginatorEndpoint;
    TransactionTransportHeader m_commonTransportHeaderOfRemoteTransaction;
    /**
     * Transaction state, we need to synchronize remote side to, before we can mark it write sync.
     */
    ::ec2::QnTranState m_tranStateToSynchronizeTo;
    /** 
     * Transaction state of remote peer. Transactions before this state have been sent to the peer.
     */
    ::ec2::QnTranState m_remotePeerTranState;
    bool m_haveToSendSyncDone;
    bool m_closed;
    std::unique_ptr<network::aio::Timer> m_inactivityTimer;

    int highestProtocolVersionCompatibleWithRemotePeer() const;

    void onGotTransaction(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        ::ec2::QnTransactionTransportHeader transportHeader);

    void forwardTransactionToProcessor(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        ::ec2::QnTransactionTransportHeader transportHeader);

    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);

    void forwardStateChangedEvent(
        ::ec2::QnTransactionTransportBase::State newState);

    void onTransactionsReadFromLog(
        api::ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransaction,
        ::ec2::QnTranState readedUpTo);

    void enableOutputChannel();

    void onInactivityTimeout();

    template<class T>
    void sendTransaction(
        ::ec2::QnTransaction<T> transaction,
        TransactionTransportHeader transportHeader);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
