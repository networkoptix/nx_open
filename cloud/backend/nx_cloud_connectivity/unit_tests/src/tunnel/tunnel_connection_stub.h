#pragma once

#include <nx/network/cloud/tunnel/abstract_outgoing_tunnel_connection.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class TunnelConnectionStub:
    public AbstractOutgoingTunnelConnection
{
public:
    ~TunnelConnectionStub();

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds /*timeout*/,
        SocketAttributes /*socketAttributes*/,
        OnNewConnectionHandler /*handler*/) override;

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual std::string toString() const override;

    bool isStarted() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onClosedHandler;
    bool m_isStarted = false;
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
