/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <atomic>
#include <list>
#include <memory>

#include <nx/network/aio/timer.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_outgoing_tunnel_connection.h"
#include "nx/network/udt/udt_socket.h"


namespace nx {
namespace network {
namespace cloud {

/** Creates connections (UDT) after UDP hole punching has been successfully done.
    Also, makes some efforts to keep UDP hole opened
    \note \a OutgoingTunnelUdtConnection instance can be safely freed in new connection handler
*/
class NX_NETWORK_API OutgoingTunnelUdtConnection
:
    public AbstractOutgoingTunnelConnection
{
public:
    /** 
        \param connectionId unique id of connection established
        \param udtConnection already established connection to the target host
    */
    OutgoingTunnelUdtConnection(
        nx::String connectionId,
        std::unique_ptr<UdtStreamSocket> udtConnection);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;

private:
    struct ConnectionContext
    {
        std::unique_ptr<UdtStreamSocket> connection;
        OnNewConnectionHandler completionHandler;
    };

    const nx::String m_connectionId;
    std::unique_ptr<UdtStreamSocket> m_controlConnection;
    const SocketAddress m_remoteHostAddress;
    aio::Timer m_aioTimer;
    std::map<UdtStreamSocket*, ConnectionContext> m_ongoingConnections;
    QnMutex m_mutex;
    std::atomic<bool> m_terminated;

    void onConnectCompleted(
        UdtStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode);
    void reportConnectResult(
        UdtStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode);
};

} // namespace cloud
} // namespace network
} // namespace nx
