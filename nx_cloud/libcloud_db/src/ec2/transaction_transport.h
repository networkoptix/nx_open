#pragma once

#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <common/common_globals.h>
#include <nx_ec/data/api_tran_state_data.h>
#include <transaction/transaction_transport_base.h>

#include "transaction_processor.h"
#include "transaction_log_reader.h"
#include "transaction_transport_header.h"
#include "transaction_serializer.h"

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

    /**
     * Initializer for incoming connection.
     */
    TransactionTransport(
        nx::network::aio::AbstractAioThread* aioThread,
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
        const std::shared_ptr<const TransactionWithSerializedPresentation>& transactionSerializer);

    template<class T>
    void sendTransaction(
        ::ec2::QnTransaction<T> transaction,
        TransactionTransportHeader transportHeader)
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Sending transaction %1 to %2").str(transaction.command)
                .str(m_commonTransportHeaderOfRemoteTransaction),
            cl_logDEBUG1);

        std::shared_ptr<const TransactionWithSerializedPresentation> transactionSerializer;
        switch (remotePeer().dataFormat)
        {
            case Qn::UbjsonFormat:
            {
                auto serializedTransaction = QnUbjson::serialized(transaction);
                transactionSerializer = 
                    std::make_unique<TransactionWithUbjsonPresentation<T>>(
                        std::move(transaction),
                        std::move(serializedTransaction));
                break;
            }

            default:
            {
                NX_LOGX(lm("Cannot send transaction in unsupported format %1 to %2")
                    .arg(QnLexical::serialized(remotePeer().dataFormat))
                    .str(m_commonTransportHeaderOfRemoteTransaction),
                    cl_logDEBUG1);
                // TODO: #ak close connection
                NX_ASSERT(false);
                return;
            }
        }

        sendTransaction(
            std::move(transportHeader),
            transactionSerializer);
    }

    const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const;

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

    void onGotTransaction(
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        ::ec2::QnTransactionTransportHeader transportHeader);
    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);
    void onTransactionsReadFromLog(
        api::ResultCode resultCode,
        std::vector<std::shared_ptr<const Serializable>> serializedTransaction,
        ::ec2::QnTranState readedUpTo);
    void enableOutputChannel();
    //void addTransportHeaderToUbjsonTransaction(
    //    nx::Buffer* const ubjsonTransaction);
    //void addTransportHeaderToJsonTransaction(QJsonObject*);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
