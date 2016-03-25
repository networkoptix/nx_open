/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <utils/common/stoppable.h>

#include "abstract_tunnel_connector.h"
#include "nx/network/aio/timer.h"
#include "nx/network/system_socket.h"
#include "tunnel.h"


namespace nx {
namespace network {
namespace cloud {

/**
    \note \a OutgoingTunnel instance can be safely freed only after 
        \a OutgoingTunnel::pleaseStop completion. It is allowed 
        to free object in \a closed handler itself.
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
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    /** Establish new connection.
    * @param timeout Zero means no timeout
    * @param socketAttributes attribute values to apply to a newly-created socket
    * \note This method is re-enterable. So, it can be called in
    *        different threads simultaneously */
    void establishNewConnection(
        std::chrono::milliseconds timeout,
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
        /** zero - no timeout */
        std::chrono::milliseconds timeout;
        NewConnectionHandler handler;
    };

    const AddressEntry m_targetPeerAddress;
    //map<connection request expiration time, connection request data>
    std::multimap<
        std::chrono::steady_clock::time_point,
        ConnectionRequestData> m_connectHandlers;
    std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>> m_connectors;
    aio::Timer m_timer;
    bool m_terminated;
    boost::optional<std::chrono::steady_clock::time_point> m_timerTargetClock;
    int m_counter;
    std::shared_ptr<AbstractOutgoingTunnelConnection> m_connection;
    SystemError::ErrorCode m_lastErrorCode;

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
    void onTunnelClosed(SystemError::ErrorCode errorCode);
    void startAsyncTunnelConnect(QnMutexLockerBase* const locker);
    void onConnectorFinished(
        CloudConnectType connectorType,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);

    void connectorsTerminated(
        nx::utils::MoveOnlyFunc<void()> pleaseStopCompletionHandler);
    void connectorsTerminatedNonSafe(
        QnMutexLockerBase* const /*lock*/,
        nx::utils::MoveOnlyFunc<void()> pleaseStopCompletionHandler);
    void connectionTerminated(
        nx::utils::MoveOnlyFunc<void()> pleaseStopCompletionHandler);
};

} // namespace cloud
} // namespace network
} // namespace nx
