/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/async_stoppable.h>

#include "abstract_tunnel_connector.h"
#include "cross_nat_connector.h"
#include "nx/network/aio/basic_pollable.h"
#include "nx/network/aio/timer.h"
#include "nx/network/system_socket.h"
#include "tunnel.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * @note OutgoingTunnel instance can be safely freed only after 
 *       OutgoingTunnel::pleaseStop completion. It is allowed 
 *       to free object in "on closed" handler itself.
 *       It is needed to guarantee that all clients receive response.
 * @note Calling party MUST not use object after OutgoingTunnel::pleaseStop call
 */
class NX_NETWORK_API OutgoingTunnel:
    public aio::BasicPollable
{
    typedef aio::BasicPollable BaseType;

public:
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> SocketHandler;

    enum class State
    {
        init,
        connecting,
        connected,
        closed
    };

    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> NewConnectionHandler;

    OutgoingTunnel(AddressEntry targetPeerAddress);
    virtual ~OutgoingTunnel();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    /**
     * @note Calling party MUST not use object after OutgoingTunnel::pleaseStop call.
     */
    virtual void stopWhileInAioThread() override;

    void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler);

    /**
     * Establish new connection.
     * @param timeout Zero means no timeout
     * @param socketAttributes attribute values to apply to a newly-created socket
     * @note This method is re-enterable. So, it can be called in
     *       different threads simultaneously
     */
    void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        NewConnectionHandler handler);
    /** Same as above, but no timeout. */
    void establishNewConnection(
        SocketAttributes socketAttributes,
        NewConnectionHandler handler);

    static QString stateToString(State state);

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
    std::unique_ptr<AbstractCrossNatConnector> m_connector;
    std::unique_ptr<aio::Timer> m_timer;
    bool m_terminated;
    boost::optional<std::chrono::steady_clock::time_point> m_timerTargetClock;
    std::unique_ptr<AbstractOutgoingTunnelConnection> m_connection;
    SystemError::ErrorCode m_lastErrorCode;
    QnMutex m_mutex;
    State m_state;
    nx::utils::MoveOnlyFunc<void()> m_onClosedHandler;

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
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
    void setTunnelConnection(std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
};

} // namespace cloud
} // namespace network
} // namespace nx
