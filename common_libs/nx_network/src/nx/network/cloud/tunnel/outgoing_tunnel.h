/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <utils/common/stoppable.h>

#include "abstract_tunnel_connector.h"
#include "nx/network/system_socket.h"
#include "tunnel.h"


namespace nx {
namespace network {
namespace cloud {

class OutgoingTunnel
:
    public Tunnel
{
public:
    typedef std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> NewConnectionHandler;

    OutgoingTunnel(AddressEntry targetPeerAddress);

    virtual void pleaseStop(std::function<void()> handler) override;

    /** Establish new connection
    * \param socketAttributes attribute values to apply to a newly-created socket
    * \note This method is re-enterable. So, it can be called in
    *        different threads simultaneously */
    void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        NewConnectionHandler handler);

private:
    struct ConnectionRequestData
    {
        SocketAttributes socketAttributes;
        boost::optional<std::chrono::milliseconds> timeout;
        NewConnectionHandler handler;
    };

    const AddressEntry m_targetPeerAddress;
    //map<connection request expiration time, connection request data>
    std::multimap<
        std::chrono::steady_clock::time_point,
        ConnectionRequestData> m_connectHandlers;
    std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>> m_connectors;
    //TODO #ak replace with aio timer when it is available
    UDPSocket m_aioThreadBinder;

    void onConnectFinished(
        NewConnectionHandler handler,
        SystemError::ErrorCode code,
        std::unique_ptr<AbstractStreamSocket> socket,
        bool stillValid);
    void onTunnelClosed();
    void startAsyncTunnelConnect(QnMutexLockerBase* const locker);
    void onConnectorFinished(
        CloudConnectType connectorType,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractTunnelConnection> connection);

    static std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>>
        createAllCloudConnectors(const AddressEntry& address);
};

} // namespace cloud
} // namespace network
} // namespace nx
