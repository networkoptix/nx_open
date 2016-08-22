/**********************************************************
* Aug 10, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <common/common_globals.h>
#include <nx_ec/data/api_tran_state_data.h>
#include <transaction/transaction_transport_base.h>

#include "transaction_processor.h"
#include "transaction_transport_header.h"


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

class TransactionTransport
:
    public ::ec2::QnTransactionTransportBase,
    public nx::network::aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> ConnectionClosedEventHandler;
    typedef nx::utils::MoveOnlyFunc<void(
        Qn::SerializationFormat,
        const QByteArray&,
        TransactionTransportHeader)> GotTransactionEventHandler;

    //!Initializer for incoming connection
    TransactionTransport(
        TransactionLog* const transactionLog,
        const nx::String& systemId,
        const nx::String& connectionId,
        const ::ec2::ApiPeerData& localPeer,
        const ::ec2::ApiPeerData& remotePeer,
        QSharedPointer<AbstractCommunicatingSocket> socket,
        const nx_http::Request& request,
        const QByteArray& contentEncoding);
    virtual ~TransactionTransport();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    /** Set handler to be executed when tcp connection is closed */
    void setOnConnectionClosed(ConnectionClosedEventHandler handler);
    void setOnGotTransaction(GotTransactionEventHandler handler);

    void processSyncRequest(
        const TransactionTransportHeader& transportHeader,
        ::ec2::ApiSyncRequestData data,
        TransactionProcessedHandler handler);

    void processSyncResponse(
        const TransactionTransportHeader& transportHeader,
        ::ec2::QnTranStateResponse data,
        TransactionProcessedHandler handler);

    void processSyncDone(
        const TransactionTransportHeader& transportHeader,
        ::ec2::ApiTranSyncDoneData data,
        TransactionProcessedHandler handler);

    template<class T>
    void sendTransaction(
        ::ec2::QnTransaction<T> transaction,
        TransactionTransportHeader transportHeader)
    {
        post([this, transaction = std::move(transaction),
                transportHeader = std::move(transportHeader)]() mutable
            {
                if (isReadyToSend(transaction.command) /*&& queue size is too large*/)  //TODO #ak check transaction to send queue size
                    setWriteSync(false);

                if (isReadyToSend(transaction.command))
                {
                    ::ec2::QnTransactionTransportBase::sendTransactionImpl<T>(
                        std::move(transaction),
                        std::move(transportHeader.vmsTransportHeader));
                    return;
                }

                //cannot send transaction right now: updating local transaction sequence
                const ::ec2::QnTranStateKey tranStateKey(
                    transaction.peerID,
                    transaction.persistentInfo.dbID);
                m_tranStateToSynchronizeTo.values[tranStateKey] =
                    transaction.persistentInfo.sequence;
                //transaction will be sent later
            });
    }

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;

private:
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    std::unique_ptr<TransactionLogReader> m_transactionLogReader;
    const nx::String m_systemId;
    const nx::String m_connectionId;
    const SocketAddress m_connectionOriginatorEndpoint;
    /** tran state, we need to synchronize remote side to, before we can mark it write sync */
    ::ec2::QnTranState m_tranStateToSynchronizeTo;
    /** tran state of remote peer. Transactions before this state have been sent to the peer */
    ::ec2::QnTranState m_remotePeerTranState;
    bool m_haveToSendSyncDone;

    void onGotTransaction(
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        ::ec2::QnTransactionTransportHeader transportHeader);
    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);
    void onTransactionsReadFromLog(
        api::ResultCode resultCode,
        std::vector<nx::Buffer> serializedTransactions);
    void addTransportHeaderToUbjsonTransaction(nx::Buffer);
    void addTransportHeaderToJsonTransaction(QJsonObject*);
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
