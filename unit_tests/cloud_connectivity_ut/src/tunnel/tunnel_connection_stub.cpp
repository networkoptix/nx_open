#include "tunnel_connection_stub.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

TunnelConnectionStub::~TunnelConnectionStub()
{
    stopWhileInAioThread();
}

void TunnelConnectionStub::start()
{
}

void TunnelConnectionStub::establishNewConnection(
    std::chrono::milliseconds /*timeout*/,
    SocketAttributes /*socketAttributes*/,
    OnNewConnectionHandler /*handler*/)
{
}

void TunnelConnectionStub::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_onClosedHandler = std::move(handler);
}

void TunnelConnectionStub::stopWhileInAioThread()
{
    auto onClosedHandler = std::move(m_onClosedHandler);
    if (onClosedHandler)
        onClosedHandler(SystemError::interrupted);
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
