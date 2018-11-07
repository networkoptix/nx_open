#include "command_transport_delegate.h"

namespace nx::data_sync_engine::transport {

CommandTransportDelegate::CommandTransportDelegate(
    AbstractTransactionTransport* delegatee)
    :
    m_delegatee(delegatee)
{
}

void CommandTransportDelegate::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_delegatee->bindToAioThread(aioThread);
}

void CommandTransportDelegate::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_delegatee->pleaseStopSync();
}

network::SocketAddress CommandTransportDelegate::remotePeerEndpoint() const
{
    return m_delegatee->remotePeerEndpoint();
}

ConnectionClosedSubscription& CommandTransportDelegate::connectionClosedSubscription()
{
    return m_delegatee->connectionClosedSubscription();
}

void CommandTransportDelegate::setOnGotTransaction(GotTransactionEventHandler handler)
{
    m_delegatee->setOnGotTransaction(std::move(handler));
}

QnUuid CommandTransportDelegate::connectionGuid() const
{
    return m_delegatee->connectionGuid();
}

const TransactionTransportHeader&
    CommandTransportDelegate::commonTransportHeaderOfRemoteTransaction() const
{
    return m_delegatee->commonTransportHeaderOfRemoteTransaction();
}

void CommandTransportDelegate::sendTransaction(
    TransactionTransportHeader transportHeader,
    const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer)
{
    m_delegatee->sendTransaction(
        std::move(transportHeader),
        transactionSerializer);
}

void CommandTransportDelegate::start()
{
    m_delegatee->start();
}

} // namespace nx::data_sync_engine::transport
