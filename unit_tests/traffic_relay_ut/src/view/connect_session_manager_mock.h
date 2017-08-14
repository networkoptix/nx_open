#pragma once

#include <nx/utils/thread/sync_queue.h>

#include <controller/connect_session_manager.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class ConnectSessionManagerMock:
    public controller::AbstractConnectSessionManager,
    public controller::AbstractReportPublicAddressHandler
{
public:
    ConnectSessionManagerMock(
        utils::SyncQueue<api::BeginListeningRequest>* receivedBeginListeningRequests,
        utils::SyncQueue<api::CreateClientSessionRequest>* receivedCreateClientSessionRequests,
        utils::SyncQueue<api::ConnectToPeerRequest>* receivedConnectToPeerRequests);

    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) override;

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) override;

    virtual void connectToPeer(
        const api::ConnectToPeerRequest& request,
        ConnectToPeerHandler completionHandler) override;

    virtual void onPublicAddressDiscovered(std::string /*publicAddress*/) override {};

private:
    utils::SyncQueue<api::BeginListeningRequest>* m_receivedBeginListeningRequests;
    utils::SyncQueue<api::CreateClientSessionRequest>* m_receivedCreateClientSessionRequests;
    utils::SyncQueue<api::ConnectToPeerRequest>* m_receivedConnectToPeerRequests;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
