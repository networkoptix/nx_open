// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "acceptor_stub.h"

namespace nx {
namespace network {
namespace test {

std::atomic<int> AcceptorStub::instanceCount(0);

AcceptorStub::AcceptorStub(
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>>* readyConnections)
    :
    m_readyConnections(readyConnections)
{
    ++instanceCount;

    bindToAioThread(getAioThread());
}

AcceptorStub::~AcceptorStub()
{
    --instanceCount;
    m_removedAcceptorsQueue->push(this);

    pleaseStopSync();
}

void AcceptorStub::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_repetitiveTimer.bindToAioThread(aioThread);
}

void AcceptorStub::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_repetitiveTimer.pleaseStopSync();
}

void AcceptorStub::acceptAsync(AcceptCompletionHandler handler)
{
    m_acceptHandler = std::move(handler);

    m_repetitiveTimer.start(
        std::chrono::milliseconds(10),
        [this]() { deliverConnectionIfAvailable(); });
}

void AcceptorStub::cancelIOSync()
{
    m_acceptHandler = nullptr;
}

std::unique_ptr<AbstractStreamSocket> AcceptorStub::getNextSocketIfAny()
{
    return nullptr;
}

void AcceptorStub::setRemovedAcceptorsQueue(
    utils::SyncQueue<AcceptorStub*>* removedAcceptorsQueue)
{
    m_removedAcceptorsQueue = removedAcceptorsQueue;
}

bool AcceptorStub::isAsyncAcceptInProgress() const
{
    return m_acceptHandler != nullptr;
}

void AcceptorStub::deliverConnectionIfAvailable()
{
    std::optional<std::unique_ptr<AbstractStreamSocket>> connection =
        m_readyConnections->pop(std::chrono::milliseconds::zero());
    if (!connection)
        return;

    m_repetitiveTimer.cancelSync();
    nx::utils::swapAndCall(m_acceptHandler, SystemError::noError, std::move(*connection));
}

//-------------------------------------------------------------------------------------------------

AcceptorDelegate::AcceptorDelegate(std::shared_ptr<cloud::AbstractConnectionAcceptor> target):
    m_target(target)
{
    bindToAioThread(m_target->getAioThread());
}

void AcceptorDelegate::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_target->bindToAioThread(aioThread);
}

void AcceptorDelegate::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_target->pleaseStopSync();
}

void AcceptorDelegate::acceptAsync(AcceptCompletionHandler handler)
{
    m_target->acceptAsync(std::move(handler));
}

void AcceptorDelegate::cancelIOSync()
{
    m_target->cancelIOSync();
}

std::unique_ptr<AbstractStreamSocket> AcceptorDelegate::getNextSocketIfAny()
{
    return m_target->getNextSocketIfAny();
}

} // namespace test
} // namespace network
} // namespace nx
