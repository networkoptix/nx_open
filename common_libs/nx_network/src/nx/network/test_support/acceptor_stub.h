#pragma once

#include <deque>

#include <nx/network/cloud/cloud_abstract_connection_acceptor.h>
#include <nx/utils/thread/mutex.h>
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
    bool isAsyncAcceptInProgress() const;
    void addReadyConnection(std::unique_ptr<AbstractStreamSocket> connection);

    static std::atomic<int> instanceCount;

private:
    utils::SyncQueue<AcceptorStub*>* m_removedAcceptorsQueue;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_acceptedConnections;
    AcceptCompletionHandler m_acceptHandler;
    mutable QnMutex m_mutex;

    void deliverConnection();
};

} // namespace test
} // namespace network
} // namespace nx
