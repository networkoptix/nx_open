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

    virtual const CommandTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;

    virtual void sendTransaction(
        CommandTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractCommand>& transactionSerializer) override;

    virtual void start() override;

    AbstractConnection& delegatee() { return *m_delegatee; }
    const AbstractConnection& delegatee() const { return *m_delegatee; }

protected:
    virtual void stopWhileInAioThread() override;

private:
    AbstractConnection* m_delegatee = nullptr;
};

//-------------------------------------------------------------------------------------------------

template<typename T>
class CommandTransportDelegateWithData:
    public CommandTransportDelegate
{
    using base_type = CommandTransportDelegate;

public:
    CommandTransportDelegateWithData(
        std::unique_ptr<AbstractConnection> delegatee,
        T data)
        :
        base_type(delegatee.get()),
        m_delegatee(std::move(delegatee)),
        m_data(std::move(data))
    {
    }

    const T& data() const
    {
        return m_data;
    }

private:
    std::unique_ptr<AbstractConnection> m_delegatee;
    const T m_data{};
};

} // namespace nx::clusterdb::engine::transport
