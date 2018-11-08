#pragma once

#include "../abstract_transaction_transport.h"

namespace nx::data_sync_engine::transport {

class NX_DATA_SYNC_ENGINE_API CommandTransportDelegate:
    public AbstractTransactionTransport
{
    using base_type = AbstractTransactionTransport;

public:
    CommandTransportDelegate(AbstractTransactionTransport* delegatee);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual network::SocketAddress remotePeerEndpoint() const override;

    virtual ConnectionClosedSubscription& connectionClosedSubscription() override;

    virtual void setOnGotTransaction(CommandHandler handler) override;

    virtual QnUuid connectionGuid() const override;

    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

    virtual void start() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    AbstractTransactionTransport* m_delegatee = nullptr;
};

} // namespace nx::data_sync_engine::transport
