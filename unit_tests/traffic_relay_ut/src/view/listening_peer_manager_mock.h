#pragma once

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relaying/listening_peer_manager.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class ListeningPeerManagerMock:
    public relaying::AbstractListeningPeerManager
{
public:
    ListeningPeerManagerMock(
        utils::SyncQueue<api::BeginListeningRequest>* receivedBeginListeningRequests);

    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) override;

private:
    utils::SyncQueue<api::BeginListeningRequest>* m_receivedBeginListeningRequests;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
