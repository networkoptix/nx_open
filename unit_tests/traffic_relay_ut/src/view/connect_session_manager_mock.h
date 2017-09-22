#pragma once

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/controller/connect_session_manager.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class ConnectSessionManagerMock:
    public controller::AbstractConnectSessionManager
{
public:
    ConnectSessionManagerMock(
        utils::SyncQueue<api::CreateClientSessionRequest>* receivedCreateClientSessionRequests,
        utils::SyncQueue<api::ConnectToPeerRequest>* receivedConnectToPeerRequests);

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) override;

    virtual void connectToPeer(
        const controller::ConnectToPeerRequestEx& request,
        ConnectToPeerHandler completionHandler) override;

private:
    //utils::SyncQueue<api::BeginListeningRequest>* m_receivedBeginListeningRequests;
    utils::SyncQueue<api::CreateClientSessionRequest>* m_receivedCreateClientSessionRequests;
    utils::SyncQueue<api::ConnectToPeerRequest>* m_receivedConnectToPeerRequests;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
