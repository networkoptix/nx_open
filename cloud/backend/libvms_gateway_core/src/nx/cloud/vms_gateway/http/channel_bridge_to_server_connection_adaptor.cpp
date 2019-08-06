#include "channel_bridge_to_server_connection_adaptor.h"

#include <nx/utils/log/log.h>

namespace nx::cloud::gateway {

BridgeToServerConnectionAdaptor::BridgeToServerConnectionAdaptor(
    std::unique_ptr<network::aio::AsyncChannelBridge> bridge)
    :
    m_bridge(std::move(bridge))
{
    bindToAioThread(m_bridge->getAioThread());
}

void BridgeToServerConnectionAdaptor::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_bridge->bindToAioThread(aioThread);
}

void BridgeToServerConnectionAdaptor::registerCloseHandler(
    OnConnectionClosedHandler handler)
{
    m_connectionClosedHandlers.push_back(std::move(handler));
}

void BridgeToServerConnectionAdaptor::start()
{
    m_bridge->start(
        [this](SystemError::ErrorCode error)
        {
            NX_VERBOSE(this, "Closing CONNECT tunnel.");
            triggerConnectionClosedEvent(error);
        });
}

void BridgeToServerConnectionAdaptor::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_bridge.reset();
}

void BridgeToServerConnectionAdaptor::triggerConnectionClosedEvent(
    SystemError::ErrorCode closeReason)
{
    auto connectionClosedHandlers = std::exchange(m_connectionClosedHandlers, {});
    for (auto& connectionCloseHandler: connectionClosedHandlers)
        connectionCloseHandler(closeReason);
}

} // namespace nx::cloud::gateway
