#include "tunnel_connector_stub.h"

#include "tunnel_connection_stub.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

TunnelConnectorStub::TunnelConnectorStub(
    const AddressEntry& address,
    int methodNumber,
    nx::utils::SyncQueue<int>* connectorInvocation,
    nx::utils::SyncQueue<int>* destructionEventReceiver)
    :
    m_address(address),
    m_methodNumber(methodNumber),
    m_connectorInvocation(connectorInvocation),
    m_destructionEventReceiver(destructionEventReceiver),
    m_behavior(Behavior::keepSilence)
{
}

TunnelConnectorStub::~TunnelConnectorStub()
{
    if (m_destructionEventReceiver)
        m_destructionEventReceiver->push(m_methodNumber);
}

int TunnelConnectorStub::getPriority() const
{
    return 0;
}

void TunnelConnectorStub::connect(
    const hpm::api::ConnectResponse& /*response*/,
    std::chrono::milliseconds /*timeout*/,
    ConnectCompletionHandler handler)
{
    if (m_connectorInvocation)
        m_connectorInvocation->push(m_methodNumber);
    m_completionHandler = std::move(handler);

    proceed();
}

const AddressEntry& TunnelConnectorStub::targetPeerAddress() const
{
    return m_address;
}

void TunnelConnectorStub::setBehavior(Behavior behavior)
{
    m_behavior = behavior;
    proceed();
}

void TunnelConnectorStub::setConnectionQueue(
    nx::utils::SyncQueue<TunnelConnectionStub*>* connectionQueue)
{
    m_connectionQueue = connectionQueue;
}

void TunnelConnectorStub::proceed()
{
    post(
        [this]()
        {
            if (!m_completionHandler)
                return;

            if (m_behavior == Behavior::keepSilence)
                return;

            reportResult();
        });
}

void TunnelConnectorStub::reportResult()
{
    switch (m_behavior)
    {
        case Behavior::reportFailure:
            nx::utils::swapAndCall(
                m_completionHandler,
                nx::hpm::api::NatTraversalResultCode::noSynFromTargetPeer,
                SystemError::noError,
                nullptr);
            break;

        case Behavior::reportSuccess:
        {
            auto connection = std::make_unique<TunnelConnectionStub>();
            if (m_connectionQueue)
                m_connectionQueue->push(connection.get());
            nx::utils::swapAndCall(
                m_completionHandler,
                nx::hpm::api::NatTraversalResultCode::ok,
                SystemError::noError,
                std::move(connection));
            break;
        }

        case Behavior::keepSilence:
            break;
    }
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
