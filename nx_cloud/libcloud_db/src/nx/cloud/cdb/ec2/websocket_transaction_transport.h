#pragma once

#include <memory>

#include <nx/network/websocket/websocket.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/p2p/p2p_connection_base.h>
#include <nx/p2p/connection_context.h>

#include <nx_ec/data/api_tran_state_data.h>

#include "abstract_transaction_transport.h"
#include "dao/abstract_transaction_data_object.h"
#include "transaction_log_reader.h"

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

class WebSocketTransactionTransport:
    public AbstractTransactionTransport,
    public nx::p2p::ConnectionBase
{
public:
    WebSocketTransactionTransport(
        nx::network::aio::AbstractAioThread* aioThread,
        TransactionLog* const transactionLog,
        const nx::String& systemId,
        const QnUuid& connectionId,
        std::unique_ptr<network::websocket::WebSocket> webSocket,
        ::ec2::ApiPeerDataEx localPeerData,
        ::ec2::ApiPeerDataEx remotePeerData);

    virtual network::SocketAddress remoteSocketAddr() const override;
    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;
    virtual void setOnGotTransaction(GotTransactionEventHandler handler) override;
    virtual QnUuid connectionGuid() const override;
    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;
    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

    virtual void fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey) override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

protected:
    virtual void setState(State state) override;
    virtual void stopWhileInAioThread() override;

private:
    int highestProtocolVersionCompatibleWithRemotePeer() const;
    void onGotMessage(
        QWeakPointer<nx::p2p::ConnectionBase> connection,
        nx::p2p::MessageType messageType,
        const QByteArray& payload);
    void onTransactionsReadFromLog(
        api::ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        ::ec2::QnTranState readedUpTo);
    void readTransactions();

private:
    TransactionTransportHeader m_commonTransactionHeader;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    std::unique_ptr<TransactionLogReader> m_transactionLogReader;

    bool m_sendHandshakeDone = false;
    bool m_tranLogRequestInProgress = false;
    ::ec2::QnTranState m_remoteSubscription; //< remote -> local subscription
    QnUuid m_connectionGuid;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
