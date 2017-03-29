#include "client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace api {

ClientToRelayConnection::ClientToRelayConnection(const SocketAddress& relayEndpoint):
    m_relayEndpoint(relayEndpoint)
{
}

ClientToRelayConnection::~ClientToRelayConnection()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void ClientToRelayConnection::startSession(
    const nx::String& /*desiredSessionId*/,
    const nx::String& /*targetPeerName*/,
    nx::utils::MoveOnlyFunc<void(ResultCode, nx::String /*sessionId*/)> /*handler*/)
{
    // TODO
}

void ClientToRelayConnection::openConnectionToTheTargetHost(
    nx::String& /*sessionId*/,
    nx::utils::MoveOnlyFunc<void(ResultCode, std::unique_ptr<AbstractStreamSocket>)> /*handler*/)
{
    // TODO
}

SystemError::ErrorCode ClientToRelayConnection::prevRequestSysErrorCode() const
{
    return SystemError::noError;
}

void ClientToRelayConnection::stopWhileInAioThread()
{
    // TODO
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
