#pragma once

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {
namespace test {

enum class RequestProcessingBehavior
{
    ignore,
    fail,
    produceLogicError,
    succeed,
};

class ClientImpl:
    public api::Client
{
    using base_type = api::Client;

public:
    ClientImpl();
    virtual ~ClientImpl() override;

    virtual void beginListening(
        const std::string& peerName,
        nx::cloud::relay::api::BeginListeningHandler completionHandler) override;
    virtual void startSession(
        const std::string& desiredSessionId,
        const std::string& /*targetPeerName*/,
        nx::cloud::relay::api::StartClientConnectSessionHandler handler) override;
    virtual void openConnectionToTheTargetHost(
        const std::string& /*sessionId*/,
        nx::cloud::relay::api::OpenRelayConnectionHandler handler) override;
    virtual QUrl url() const override;
    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

    int scheduledRequestCount() const;
    void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler);

    void setBehavior(RequestProcessingBehavior behavior);
    void setIgnoreRequests();
    void setFailRequests();

private:
    int m_scheduledRequestCount;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestruction;
    RequestProcessingBehavior m_behavior = RequestProcessingBehavior::succeed;

    virtual void stopWhileInAioThread() override;
};

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
