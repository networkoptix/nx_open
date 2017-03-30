#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "relay_api_result_code.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace api {

//-------------------------------------------------------------------------------------------------
// ClientToRelayConnection

using StartClientConnectSessionHandler = 
    nx::utils::MoveOnlyFunc<void(ResultCode, nx::String /*sessionId*/)>;

using OpenRelayConnectionHandler = 
    nx::utils::MoveOnlyFunc<void(ResultCode, std::unique_ptr<AbstractStreamSocket>)>;

class NX_NETWORK_API ClientToRelayConnection:
    public aio::BasicPollable
{
public:
    virtual void startSession(
        const nx::String& desiredSessionId,
        const nx::String& targetPeerName,
        StartClientConnectSessionHandler handler) = 0;
    /**
     * After successful call socket provided represents connection to the requested peer.
     */
    virtual void openConnectionToTheTargetHost(
        nx::String& sessionId,
        OpenRelayConnectionHandler handler) = 0;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const = 0;
};

//-------------------------------------------------------------------------------------------------
// ClientToRelayConnectionFactory

class NX_NETWORK_API ClientToRelayConnectionFactory
{
public:
    using CustomFactoryFunc = 
        nx::utils::MoveOnlyFunc<std::unique_ptr<ClientToRelayConnection>(const SocketAddress&)>;

    static std::unique_ptr<ClientToRelayConnection> create(const SocketAddress& relayEndpoint);

    static CustomFactoryFunc setCustomFactoryFunc(CustomFactoryFunc newFactoryFunc);
};

//-------------------------------------------------------------------------------------------------
// ClientToRelayConnectionImpl

class NX_NETWORK_API ClientToRelayConnectionImpl:
    public ClientToRelayConnection
{
public:
    ClientToRelayConnectionImpl(const SocketAddress& relayEndpoint);
    virtual ~ClientToRelayConnectionImpl() override;

    virtual void startSession(
        const nx::String& desiredSessionId,
        const nx::String& targetPeerName,
        StartClientConnectSessionHandler handler) override;

    virtual void openConnectionToTheTargetHost(
        nx::String& sessionId,
        OpenRelayConnectionHandler handler) override;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

private:
    const SocketAddress m_relayEndpoint;

    virtual void stopWhileInAioThread() override;
};

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
