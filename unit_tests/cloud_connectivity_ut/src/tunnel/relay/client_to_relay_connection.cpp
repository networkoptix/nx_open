#include "client_to_relay_connection.h"

#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

ClientToRelayConnection::ClientToRelayConnection():
    m_scheduledRequestCount(0),
    m_ignoreRequests(false),
    m_failRequests(false)
{
}

ClientToRelayConnection::~ClientToRelayConnection()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();

    // Notifying test fixture
    auto handler = std::move(m_onBeforeDestruction);
    if (handler)
        handler();
}

void ClientToRelayConnection::startSession(
    const nx::String& desiredSessionId,
    const nx::String& /*targetPeerName*/,
    api::StartClientConnectSessionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_ignoreRequests)
        return;

    post(
        [this, handler = std::move(handler), desiredSessionId]()
        {
            if (m_failRequests)
                handler(api::ResultCode::networkError, desiredSessionId);
            else
                handler(api::ResultCode::ok, desiredSessionId);
        });
}

void ClientToRelayConnection::openConnectionToTheTargetHost(
    const nx::String& /*sessionId*/,
    api::OpenRelayConnectionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_ignoreRequests)
        return;

    post(
        [this, handler = std::move(handler)]()
        {
            if (m_failRequests)
                handler(api::ResultCode::networkError, nullptr);
            else
                handler(api::ResultCode::ok, std::make_unique<nx::network::TCPSocket>());
        });
}

SystemError::ErrorCode ClientToRelayConnection::prevRequestSysErrorCode() const
{
    return SystemError::noError;
}

int ClientToRelayConnection::scheduledRequestCount() const
{
    return m_scheduledRequestCount;
}

void ClientToRelayConnection::setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestruction = std::move(handler);
}

void ClientToRelayConnection::setIgnoreRequests(bool val)
{
    m_ignoreRequests = val;
}

void ClientToRelayConnection::setFailRequests(bool val)
{
    m_failRequests = val;
}

void ClientToRelayConnection::stopWhileInAioThread()
{
    // TODO
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
