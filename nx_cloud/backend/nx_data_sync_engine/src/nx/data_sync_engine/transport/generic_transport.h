#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/fixed_size_message_pipeline.h>

#include "../abstract_transaction_transport.h"
#include "../transaction_processor.h"
#include "../transaction_log_reader.h"
#include "../transaction_transport_header.h"

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
    /*
     * TODO: #ak Introduce another type for commandPipeline.
     */
    GenericTransport(
        std::unique_ptr<AbstractTransactionTransport> commandPipeline);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual network::SocketAddress remoteSocketAddr() const override;

    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;

    virtual void setOnGotTransaction(GotTransactionEventHandler handler) override;

    virtual QnUuid connectionGuid() const override;

    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

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

    AbstractTransactionTransport& commandPipeline();

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractTransactionTransport> m_commandPipeline;
    GotTransactionEventHandler m_commandHandler;

    void handleHandshakeCommands(
        Qn::SerializationFormat serializationFormat,
        const QByteArray& serializedData,
        TransactionTransportHeader transportHeader);
};

} // namespace nx::data_sync_engine::transport
