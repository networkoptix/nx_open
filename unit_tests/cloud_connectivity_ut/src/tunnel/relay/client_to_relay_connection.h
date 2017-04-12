#pragma once

#include <nx/network/cloud/tunnel/relay/api/client_to_relay_connection.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

class ClientToRelayConnection:
    public nx::cloud::relay::api::ClientToRelayConnection
{
    using base_type = nx::cloud::relay::api::ClientToRelayConnection;

public:
    ClientToRelayConnection();
    virtual ~ClientToRelayConnection() override;

    virtual void startSession(
        const nx::String& desiredSessionId,
        const nx::String& /*targetPeerName*/,
        nx::cloud::relay::api::StartClientConnectSessionHandler handler) override;
    virtual void openConnectionToTheTargetHost(
        const nx::String& /*sessionId*/,
        nx::cloud::relay::api::OpenRelayConnectionHandler handler) override;
    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

    int scheduledRequestCount() const;
    void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler);
    void setIgnoreRequests(bool val);
    void setFailRequests(bool val);

private:
    int m_scheduledRequestCount;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestruction;
    bool m_ignoreRequests;
    bool m_failRequests;

    virtual void stopWhileInAioThread() override;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
