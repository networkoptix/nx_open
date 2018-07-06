#pragma once

#include <tuple>

#include <nx/network/cloud/tunnel/connector_factory.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

template<typename SocketType, typename... OutgoingTunnelConnectionArgs>
class OutgoingTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
public:
    OutgoingTunnelConnection(std::tuple<OutgoingTunnelConnectionArgs...> args):
        m_args(args)
    {
    }

    virtual ~OutgoingTunnelConnection() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void stopWhileInAioThread() override
    {
    }

    virtual void start() override
    {
    }

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override
    {
        auto socket = std::make_unique<SocketType>(m_args);
        socket->bindToAioThread(getAioThread());
        ASSERT_TRUE(socket->setNonBlockingMode(true));
        ASSERT_TRUE(socket->setSendTimeout(timeout.count()));

        auto serverAddress = std::get<0>(m_args);

        auto socketPtr = socket.get();
        socketPtr->connectAsync(
            serverAddress,
            [handler = std::move(handler),
            socket = std::move(socket),
            socketAttributes = std::move(socketAttributes)](
                SystemError::ErrorCode sysErrorCode) mutable
            {
                socket->cancelIOSync(aio::etNone);
                ASSERT_TRUE(socketAttributes.applyTo(socket.get()));
                handler(sysErrorCode, std::move(socket), true);
            });
    }

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_onClosedHandler = std::move(handler);
    }

    virtual std::string toString() const override
    {
        return "test::OutgoingTunnelConnection";
    }

private:
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onClosedHandler;
    std::tuple<OutgoingTunnelConnectionArgs...> m_args;
};

template<typename OutgoingTunnelConnectionType, typename... OutgoingTunnelConnectionArgs>
class CrossNatConnector:
    public AbstractCrossNatConnector
{
public:
    CrossNatConnector(OutgoingTunnelConnectionArgs... args):
        m_outgoingTunnelConnectionArgs(args...)
    {
    }

    virtual ~CrossNatConnector() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void stopWhileInAioThread() override
    {
    }

    virtual void connect(
        std::chrono::milliseconds /*timeout*/,
        ConnectCompletionHandler handler) override
    {
        post(
            [this, handler = std::move(handler)]()
            {
                auto tunnelConnection = std::make_unique<OutgoingTunnelConnectionType>(
                    m_outgoingTunnelConnectionArgs);
                tunnelConnection->bindToAioThread(getAioThread());
                handler(SystemError::noError, std::move(tunnelConnection));
            });
    }

    virtual QString getRemotePeerName() const override
    {
        return QString();
    }

private:
    std::tuple<OutgoingTunnelConnectionArgs...> m_outgoingTunnelConnectionArgs;
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
