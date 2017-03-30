#include "client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace api {

//-------------------------------------------------------------------------------------------------
// ClientToRelayConnectionFactory

static ClientToRelayConnectionFactory::CustomFactoryFunc factoryFunc;

std::unique_ptr<ClientToRelayConnection> ClientToRelayConnectionFactory::create(
    const SocketAddress& relayEndpoint)
{
    if (factoryFunc)
        return factoryFunc(relayEndpoint);
    return std::make_unique<ClientToRelayConnectionImpl>(relayEndpoint);
}

ClientToRelayConnectionFactory::CustomFactoryFunc 
    ClientToRelayConnectionFactory::setCustomFactoryFunc(CustomFactoryFunc newFactoryFunc)
{
    CustomFactoryFunc oldFunc;
    oldFunc.swap(factoryFunc);
    factoryFunc = std::move(newFactoryFunc);
    return oldFunc;
}

//-------------------------------------------------------------------------------------------------
// ClientToRelayConnectionImpl

ClientToRelayConnectionImpl::ClientToRelayConnectionImpl(const SocketAddress& relayEndpoint):
    m_relayEndpoint(relayEndpoint)
{
}

ClientToRelayConnectionImpl::~ClientToRelayConnectionImpl()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void ClientToRelayConnectionImpl::startSession(
    const nx::String& /*desiredSessionId*/,
    const nx::String& /*targetPeerName*/,
    StartClientConnectSessionHandler /*handler*/)
{
    // TODO
}

void ClientToRelayConnectionImpl::openConnectionToTheTargetHost(
    nx::String& /*sessionId*/,
    OpenRelayConnectionHandler /*handler*/)
{
    // TODO
}

SystemError::ErrorCode ClientToRelayConnectionImpl::prevRequestSysErrorCode() const
{
    return SystemError::noError;
}

void ClientToRelayConnectionImpl::stopWhileInAioThread()
{
    // TODO
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
