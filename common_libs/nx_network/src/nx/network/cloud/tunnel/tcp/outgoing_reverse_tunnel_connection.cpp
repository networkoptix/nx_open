#include "outgoing_reverse_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

static const std::chrono::seconds kCloseTunnelWhenInactive(10);

OutgoingReverseTunnelConnection::OutgoingReverseTunnelConnection(
    aio::AbstractAioThread* aioThread,
    std::shared_ptr<ReverseConnectionHolder> connectionHolder)
:
    AbstractOutgoingTunnelConnection(aioThread),
    m_connectionHolder(std::move(connectionHolder)),
    m_lastSuccessfulConnect(std::chrono::steady_clock::now())
{
    if (aioThread)
        m_timer.bindToAioThread(aioThread);
}


void OutgoingReverseTunnelConnection::stopWhileInAioThread()
{
    m_connectionHolder->clearConnectionHandler();
    m_timer.pleaseStopSync();
}

void OutgoingReverseTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
}

void OutgoingReverseTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    NX_LOGX(lm("Request for new socket with timeout: %1").str(timeout), cl_logDEBUG1);
    m_socketAttributes = std::move(socketAttributes);
    m_connectionHandler = std::move(handler);

    m_connectionHolder->takeConnection(
        [this](std::unique_ptr<AbstractStreamSocket> socket)
        {
            m_timer.post(
                [this, socket = std::move(socket)]() mutable
                {
                    NX_LOGX(lm("Got new socket(%1)").arg(socket), cl_logDEBUG1);
                    m_lastSuccessfulConnect = std::chrono::steady_clock::now();
                    closeIfInactive();

                    m_socketAttributes.applyTo(socket.get());
                    auto handler = m_connectionHandler;
                    handler(SystemError::noError, std::move(socket), true);
                });
        });

    if (!timeout.count())
        return;

    m_timer.start(
        timeout,
        [this]()
        {
            NX_LOGX(lm("Could not get socket in time"), cl_logDEBUG1);
            m_connectionHolder->clearConnectionHandler();
            closeIfInactive();

            auto handler = m_connectionHandler;
            handler(SystemError::timedOut, nullptr, true);
        });
}

void OutgoingReverseTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_closedHandler = std::move(handler);
}

void OutgoingReverseTunnelConnection::closeIfInactive()
{
    m_timer.pleaseStopSync();
    const auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_lastSuccessfulConnect + kCloseTunnelWhenInactive - std::chrono::steady_clock::now());

    if (timeout.count() > 0)
        return m_timer.start(timeout, [this](){ closeIfInactive(); });

    m_timer.post(
        [this]()
        {
            NX_LOGX(lm("Connection has expired"), cl_logDEBUG1);
            if (const auto handler = std::move(m_closedHandler))
                handler(SystemError::timedOut);
        });
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
