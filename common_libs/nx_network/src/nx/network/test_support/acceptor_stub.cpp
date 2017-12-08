#include "acceptor_stub.h"

namespace nx {
namespace network {
namespace test {

std::atomic<int> AcceptorStub::instanceCount(0);

AcceptorStub::AcceptorStub()
{
    ++instanceCount;
}

AcceptorStub::~AcceptorStub()
{
    --instanceCount;
    m_removedAcceptorsQueue->push(this);
}

void AcceptorStub::acceptAsync(AcceptCompletionHandler handler)
{
    m_acceptHandler = std::move(handler);
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

void AcceptorStub::addReadyConnection(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    QnMutexLocker lock(&m_mutex);
    m_acceptedConnections.push_back(std::move(connection));
    deliverConnection();
}

void AcceptorStub::deliverConnection()
{
    auto connection = std::move(m_acceptedConnections.front());
    m_acceptedConnections.pop_front();
    decltype(m_acceptHandler) acceptHandler;
    acceptHandler.swap(m_acceptHandler);

    post(
        [this, connection = std::move(connection),
            acceptHandler = std::move(acceptHandler)]() mutable
        {
            acceptHandler(SystemError::noError, std::move(connection));
        });
}

} // namespace test
} // namespace network
} // namespace nx
