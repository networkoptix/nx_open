#pragma once

#include <nx/utils/thread/sync_queue.h>

#include "../abstract_acceptor.h"

namespace nx {
namespace network {
namespace test {

class NX_NETWORK_API AcceptorStub:
    public AbstractAcceptor
{
public:
    AcceptorStub();
    ~AcceptorStub();

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

    void setRemovedAcceptorsQueue(
        utils::SyncQueue<AcceptorStub*>* removedAcceptorsQueue);

    static std::atomic<int> instanceCount;

private:
    utils::SyncQueue<AcceptorStub*>* m_removedAcceptorsQueue;
};

} // namespace test
} // namespace network
} // namespace nx
