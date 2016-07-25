/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <list>

#include "nx/network/system_socket.h"
#include "../abstract_tunnel_connector.h"


namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
    Currently, this class tests that tcp-connection can be established to reported endpoint.
    No remote peer validation is performed!
*/
class NX_NETWORK_API ForwardedEndpointConnector
    :
    public AbstractTunnelConnector
{
public:
    ForwardedEndpointConnector(
        AddressEntry targetHostAddress,
        nx::String connectSessionId);
    virtual ~ForwardedEndpointConnector();

    virtual void stopWhileInAioThread() override;

    virtual int getPriority() const override;
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    struct ConnectionContext
    {
        SocketAddress endpoint;
        std::unique_ptr<TCPSocket> connection;
    };

    const AddressEntry m_targetHostAddress;
    const nx::String m_connectSessionId;
    ConnectCompletionHandler m_completionHandler;
    std::list<ConnectionContext> m_connections;

    void onConnected(
        SystemError::ErrorCode sysErrorCode,
        std::list<ConnectionContext>::iterator socketIter);
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
