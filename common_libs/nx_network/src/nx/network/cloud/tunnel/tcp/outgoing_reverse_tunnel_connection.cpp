#include "outgoing_reverse_tunnel_connection.h"
#include "reverse_connection_holder.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

static const std::chrono::minutes kCloseTunnelWhenInactive(10);

OutgoingReverseTunnelConnection::OutgoingReverseTunnelConnection(
    aio::AbstractAioThread* aioThread,
    std::shared_ptr<ReverseConnectionHolder> connectionHolder)
:
    AbstractOutgoingTunnelConnection(aioThread),
    m_connectionHolder(std::move(connectionHolder))
{
}

OutgoingReverseTunnelConnection::~OutgoingReverseTunnelConnection()
{
    stopWhileInAioThread();
}

void OutgoingReverseTunnelConnection::stopWhileInAioThread()
{
    m_asyncGuard.reset();
}

void OutgoingReverseTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    NX_LOGX(lm("Request for new socket with timeout: %1").str(timeout), cl_logDEBUG1);
    m_connectionHolder->takeSocket(
        timeout,
        [this, guard = m_asyncGuard.sharedGuard(), timeout = std::move(timeout),
            socketAttributes = std::move(socketAttributes), handler = std::move(handler)](
                SystemError::ErrorCode code,
                std::unique_ptr<AbstractStreamSocket> socket)
        {
            if (auto lock = guard->lock())
            {
                NX_LOGX(lm("Got new socket(%1): %2")
                    .args(socket, SystemError::toString(code)), cl_logDEBUG1);

                if (code == SystemError::noError)
                {
                    socketAttributes.applyTo(socket.get());
                    timer()->start(
                        kCloseTunnelWhenInactive,
                        [this]()
                        {
                            NX_LOGX(lm("Close tunnel by inactivity timer after %1")
                                .str(kCloseTunnelWhenInactive), cl_logDEBUG1);

                            if (const auto handler = std::move(m_closedHandler))
                                handler(SystemError::timedOut);
                        });
                }

                handler(code, std::move(socket), true);
            }
        });
}

void OutgoingReverseTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_closedHandler = std::move(handler);
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
