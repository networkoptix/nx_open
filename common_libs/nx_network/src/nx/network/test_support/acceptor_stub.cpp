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

void AcceptorStub::acceptAsync(AcceptCompletionHandler /*handler*/)
{
}

void AcceptorStub::cancelIOSync()
{
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

} // namespace test
} // namespace network
} // namespace nx
