#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/fixed_size_message_pipeline.h>

#include "../abstract_transaction_transport.h"
#include "../compatible_ec2_protocol_version.h"
#include "../transaction_processor.h"
#include "../transaction_log_reader.h"
#include "../transaction_transport_header.h"

namespace nx::data_sync_engine { class TransactionLog; }

namespace nx::data_sync_engine::transport {

/**
 * Synchronizes two pees over command pipeline.
 * Serialization / deserialization and network transport details 
 * are out of scope of this class and encapsulated by commandPipeline.
 */
class GenericTransport:
    public AbstractTransactionTransport
{
    using base_type = AbstractTransactionTransport;

public:
    GenericTransport(
        const ProtocolVersionRange& protocolVersionRange,
        TransactionLog* const transactionLog,
        const OutgoingCommandFilter& outgoingCommandFilter,
        const std::string& systemId,
        const ConnectionRequestAttributes& connectionRequestAttributes,
        const vms::api::PeerData& localPeer,
        std::unique_ptr<AbstractCommandPipeline> commandPipeline);

    virtual ~GenericTransport();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual network::SocketAddress remotePeerEndpoint() const override;

    virtual ConnectionClosedSubscription& connectionClosedSubscription() override;

    virtual void setOnGotTransaction(CommandHandler handler) override;

    virtual QnUuid connectionGuid() const override;

    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

    virtual void start() override;

    AbstractCommandPipeline& commandPipeline();

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange m_protocolVersionRange;
    const std::string m_systemId;
    const vms::api::PeerData m_localPeer;
    const vms::api::PeerData m_remotePeer;
    std::unique_ptr<AbstractCommandPipeline> m_commandPipeline;
    bool m_canSendCommands = false;
    /**
     * Transaction state, we need to synchronize remote side to, before we can mark it write sync.
     */
    vms::api::TranState m_tranStateToSynchronizeTo;
    /**
     * Transaction state of remote peer. Transactions before this state have been sent to the peer.
     */
    vms::api::TranState m_remotePeerTranState;
    bool m_haveToSendSyncDone = false;
    std::unique_ptr<TransactionLogReader> m_transactionLogReader;
    TransactionTransportHeader m_commonTransportHeaderOfRemoteTransaction;
    ConnectionClosedSubscription m_connectionClosedSubscription;
    CommandHandler m_gotCommandHandler;

    void processConnectionClosedEvent(SystemError::ErrorCode closeReason);

    void processCommandData(
        Qn::SerializationFormat serializationFormat,
        const QByteArray& serializedCommand,
        TransactionTransportHeader transportHeader);

    bool isHandshakeCommand(int commandType) const;

    void processHandshakeCommand(
        TransactionTransportHeader transportHeader,
        std::unique_ptr<DeserializableCommandData> commandData);

    void processHandshakeCommand(
        Command<vms::api::SyncRequestData> data);

    void onTransactionsReadFromLog(
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransaction,
        vms::api::TranState readedUpTo);

    void enableOutputChannel();
};

} // namespace nx::data_sync_engine::transport
