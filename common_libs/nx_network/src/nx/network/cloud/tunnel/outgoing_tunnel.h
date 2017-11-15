#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/system_socket.h>
#include <nx/utils/basic_factory.h>

#include "abstract_tunnel_connector.h"
#include "abstract_cross_nat_connector.h"
#include "tunnel.h"
#include "tunnel_attributes.h"

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API AbstractOutgoingTunnel:
    public aio::BasicPollable
{
public:
    using NewConnectionHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        TunnelAttributes tunnelAttributes,
        std::unique_ptr<AbstractStreamSocket>)>;

    static constexpr std::chrono::milliseconds kNoTimeout = std::chrono::milliseconds::zero();

    virtual ~AbstractOutgoingTunnel() = default;

    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * Establish new connection.
     * @param timeout Zero means no timeout.
     * @param socketAttributes attribute values to apply to a newly-created socket.
     * NOTE: This method is re-enterable. So, it can be called in different threads simultaneously.
     */
    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        NewConnectionHandler handler) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * NOTE: OutgoingTunnel instance can be safely freed only after
 *       OutgoingTunnel::pleaseStop completion. It is allowed
 *       to free object in "on closed" handler itself.
 *       It is needed to guarantee that all clients receive response.
 * NOTE: Calling party MUST not use object after OutgoingTunnel::pleaseStop call
 */
class NX_NETWORK_API OutgoingTunnel:
    public AbstractOutgoingTunnel
{
    using base_type = AbstractOutgoingTunnel;

public:
    using SocketHandler = nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)>;

    enum class State
    {
        init,
        connecting,
        connected,
        closed
    };

    OutgoingTunnel(AddressEntry targetPeerAddress);
    virtual ~OutgoingTunnel();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    /**
     * NOTE: Calling party MUST not use object after OutgoingTunnel::pleaseStop call.
     */
    virtual void stopWhileInAioThread() override;

    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        NewConnectionHandler handler) override;

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
    TunnelAttributes m_attributes;

    void postponeConnectTask(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        NewConnectionHandler handler);
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

//-------------------------------------------------------------------------------------------------

using OutgoingTunnelFactoryFunction = std::unique_ptr<AbstractOutgoingTunnel>(const AddressEntry&);

class NX_NETWORK_API OutgoingTunnelFactory:
    public nx::utils::BasicFactory<OutgoingTunnelFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<OutgoingTunnelFactoryFunction>;

public:
    OutgoingTunnelFactory();

    static OutgoingTunnelFactory& instance();

private:
    std::unique_ptr<AbstractOutgoingTunnel> defaultFactoryFunction(
        const AddressEntry& address);
};

} // namespace cloud
} // namespace network
} // namespace nx
