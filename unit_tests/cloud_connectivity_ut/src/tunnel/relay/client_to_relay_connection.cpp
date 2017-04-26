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

void ClientToRelayConnection::beginListening(
    const nx::String& /*peerName*/,
    nx::cloud::relay::api::BeginListeningHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_ignoreRequests)
        return;

    post(
        [this, handler = std::move(handler)]()
        {
            nx::cloud::relay::api::BeginListeningResponse response;
            response.preemptiveConnectionCount = 7;
            if (m_failRequests)
                handler(nx::cloud::relay::api::ResultCode::networkError, std::move(response));
            else
                handler(nx::cloud::relay::api::ResultCode::ok, std::move(response));
        });
}

void ClientToRelayConnection::startSession(
    const nx::String& desiredSessionId,
    const nx::String& /*targetPeerName*/,
    nx::cloud::relay::api::StartClientConnectSessionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_ignoreRequests)
        return;

    post(
        [this, handler = std::move(handler), desiredSessionId]()
        {
            nx::cloud::relay::api::CreateClientSessionResponse response;
            response.sessionId = desiredSessionId;
            if (m_failRequests)
                handler(nx::cloud::relay::api::ResultCode::networkError, std::move(response));
            else
                handler(nx::cloud::relay::api::ResultCode::ok, std::move(response));
        });
}

void ClientToRelayConnection::openConnectionToTheTargetHost(
    const nx::String& /*sessionId*/,
    nx::cloud::relay::api::OpenRelayConnectionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_ignoreRequests)
        return;

    post(
        [this, handler = std::move(handler)]()
        {
            if (m_failRequests)
            {
                handler(
                    nx::cloud::relay::api::ResultCode::networkError,
                    nullptr);
            }
            else
            {
                handler(
                    nx::cloud::relay::api::ResultCode::ok,
                    std::make_unique<nx::network::TCPSocket>());
            }
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
