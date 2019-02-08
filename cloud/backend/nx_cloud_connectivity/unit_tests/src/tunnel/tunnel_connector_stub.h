#pragma once

#include <nx/network/cloud/tunnel/abstract_tunnel_connector.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class TunnelConnectionStub;

class TunnelConnectorStub:
    public AbstractTunnelConnector
{
public:
    enum class Behavior
    {
        keepSilence,
        reportSuccess,
        reportFailure,
    };

    TunnelConnectorStub(
        const AddressEntry& address,
        int methodNumber = 0,
        nx::utils::SyncQueue<int>* connectorInvocation = nullptr,
        nx::utils::SyncQueue<int>* destructionEventReceiver = nullptr);
    ~TunnelConnectorStub();

    virtual int getPriority() const override;
    virtual void connect(
        const hpm::api::ConnectResponse& /*response*/,
        std::chrono::milliseconds /*timeout*/,
        ConnectCompletionHandler handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

    void setBehavior(Behavior behavior);
    void setConnectionQueue(nx::utils::SyncQueue<TunnelConnectionStub*>* connectionQueue);

private:
    const AddressEntry m_address;
    int m_methodNumber;
    nx::utils::SyncQueue<int>* m_connectorInvocation;
    nx::utils::SyncQueue<int>* m_destructionEventReceiver;
    nx::utils::SyncQueue<TunnelConnectionStub*>* m_connectionQueue = nullptr;
    ConnectCompletionHandler m_completionHandler;
    Behavior m_behavior;

    void proceed();
    void reportResult();
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
