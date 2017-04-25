#pragma once

#include <controller/connect_session_manager.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class ConnectSessionManagerMock:
    public controller::AbstractConnectSessionManager
{
public:
    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) override;

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) override;

    virtual void connectToPeer(
        const api::ConnectToPeerRequest& request,
        ConnectToPeerHandler completionHandler) override;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
