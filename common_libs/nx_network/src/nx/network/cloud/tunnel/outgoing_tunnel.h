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

/**
    \note \a OutgoingTunnel instance can be safely freed only after 
        \a OutgoingTunnel::pleaseStop completion. It is allowed 
        to free object in completion handler itself.
        It is needed to guarantee that all clients receive response.
    \note Calling party MUST not use object after \a OutgoingTunnel::pleaseStop call
*/
class NX_NETWORK_API OutgoingTunnel
:
    public Tunnel
{
public:
    typedef std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> NewConnectionHandler;

    OutgoingTunnel(AddressEntry targetPeerAddress);
    virtual ~OutgoingTunnel();

    /**
       \note Calling party MUST not use object after \a OutgoingTunnel::pleaseStop call
     */
    virtual void pleaseStop(std::function<void()> handler) override;

    /** Establish new connection.
    * \param socketAttributes attribute values to apply to a newly-created socket
    * \note This method is re-enterable. So, it can be called in
    *        different threads simultaneously */
    void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        NewConnectionHandler handler);
    /** same as above, but no timeout */
    void establishNewConnection(
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
    bool m_terminated;
    boost::optional<std::chrono::steady_clock::time_point> m_timerTargetClock;
    int m_counter;

    void updateTimerIfNeeded();
    void updateTimerIfNeededNonSafe(
        QnMutexLockerBase* const /*lock*/,
        const std::chrono::steady_clock::time_point curTime);
    void onTimer();
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

    void connectorsTerminated(
        std::function<void()> pleaseStopCompletionHandler);
    void connectorsTerminatedNonSafe(
        QnMutexLockerBase* const /*lock*/,
        std::function<void()> pleaseStopCompletionHandler);
    void connectionTerminated(
        std::function<void()> pleaseStopCompletionHandler);
};

} // namespace cloud
} // namespace network
} // namespace nx
