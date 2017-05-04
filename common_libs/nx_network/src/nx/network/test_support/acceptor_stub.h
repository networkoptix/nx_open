#pragma once

#include <nx/network/cloud/cloud_abstract_connection_acceptor.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

class NX_NETWORK_API AcceptorStub:
    public cloud::AbstractConnectionAcceptor
{
public:
    AcceptorStub();
    ~AcceptorStub();

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;

    void setRemovedAcceptorsQueue(
        utils::SyncQueue<AcceptorStub*>* removedAcceptorsQueue);

    static std::atomic<int> instanceCount;

private:
    utils::SyncQueue<AcceptorStub*>* m_removedAcceptorsQueue;
};

} // namespace test
} // namespace network
} // namespace nx
