#pragma once

#include <memory>

#include "abstract_transaction_transport.h"
#include "dao/abstract_transaction_data_object.h"
#include "transaction_log_reader.h"

#include <nx/network/websocket/websocket.h>
#include <nx/p2p/p2p_connection_base.h>
#include <nx/p2p/connection_context.h>
#include <nx/vms/api/data/tran_state_data.h>

namespace nx {
namespace data_sync_engine {

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
        vms::api::PeerDataEx localPeerData,
        vms::api::PeerDataEx remotePeerData);

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
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo);

    void readTransactions();

private:
    TransactionTransportHeader m_commonTransactionHeader;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    std::unique_ptr<TransactionLogReader> m_transactionLogReader;

    bool m_sendHandshakeDone = false;
    bool m_tranLogRequestInProgress = false;
    vms::api::TranState m_remoteSubscription; //< remote -> local subscription
    QnUuid m_connectionGuid;
};

} // namespace data_sync_engine
} // namespace nx
