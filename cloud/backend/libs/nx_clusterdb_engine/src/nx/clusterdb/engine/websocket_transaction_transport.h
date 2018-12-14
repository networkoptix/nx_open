#pragma once

#include <optional>
#include <memory>

#include "abstract_transaction_transport.h"
#include "dao/abstract_transaction_data_object.h"
#include "transaction_log_reader.h"

#include <nx/network/websocket/websocket.h>
#include <nx/p2p/p2p_connection_base.h>
#include <nx/p2p/connection_context.h>
#include <nx/vms/api/data/tran_state_data.h>

#include "compatible_ec2_protocol_version.h"

namespace nx::clusterdb::engine { class CommandLog; }

namespace nx::clusterdb::engine::transport {

class WebsocketCommandTransport:
    public AbstractConnection,
    public nx::p2p::ConnectionBase
{
public:
    WebsocketCommandTransport(
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* const transactionLog,
        const std::string& systemId,
        const OutgoingCommandFilter& filter,
        const std::string& connectionId,
        nx::network::P2pTransportPtr p2pTransport,
        vms::api::PeerDataEx localPeerData,
        vms::api::PeerDataEx remotePeerData);

    virtual network::SocketAddress remotePeerEndpoint() const override;
    virtual ConnectionClosedSubscription& connectionClosedSubscription() override;
    virtual void setOnGotTransaction(CommandHandler handler) override;
    virtual std::string connectionGuid() const override;
    virtual const CommandTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;
    virtual void sendTransaction(
        CommandTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractCommand>& transactionSerializer) override;
    virtual void start() override;

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

    void reportCommandReceived(QByteArray commandBuffer);

    void onTransactionsReadFromLog(
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo);

    void readTransactions();

private:
    const ProtocolVersionRange m_protocolVersionRange;
    CommandTransportHeader m_commonTransactionHeader;
    ConnectionClosedSubscription m_connectionClosedSubscription;
    CommandHandler m_gotTransactionEventHandler;
    std::unique_ptr<CommandLogReader> m_transactionLogReader;

    bool m_sendHandshakeDone = false;
    bool m_tranLogRequestInProgress = false;
    std::optional<vms::api::TranState> m_remoteSubscription; //< remote -> local subscription
    const std::string m_systemId;
    std::string m_connectionGuid;
};

} // namespace nx::clusterdb::engine::transport
