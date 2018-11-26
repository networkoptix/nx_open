#pragma once

#include "../abstract_transaction_transport.h"

namespace nx::clusterdb::engine::transport {

class NX_DATA_SYNC_ENGINE_API CommandTransportDelegate:
    public AbstractConnection
{
    using base_type = AbstractConnection;

public:
    CommandTransportDelegate(AbstractConnection* delegatee);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual network::SocketAddress remotePeerEndpoint() const override;

    virtual ConnectionClosedSubscription& connectionClosedSubscription() override;

    virtual void setOnGotTransaction(CommandHandler handler) override;

    virtual std::string connectionGuid() const override;

    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

    virtual void start() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    AbstractConnection* m_delegatee = nullptr;
};

} // namespace nx::clusterdb::engine::transport
