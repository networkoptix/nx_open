#pragma once

#include <nx/network/stun/server_connection.h>
#include <nx/network/system_socket.h>

namespace nx {
namespace hpm {
namespace test {

template<typename Socket>
struct SocketTypeToProtocolType
{
};

template<> struct SocketTypeToProtocolType<nx::network::TCPSocket>
{
    static const nx::network::TransportProtocol value =
        nx::network::TransportProtocol::tcp;
};

template<> struct SocketTypeToProtocolType<nx::network::UDPSocket>
{
    static const nx::network::TransportProtocol value =
        nx::network::TransportProtocol::udp;
};

//-------------------------------------------------------------------------------------------------

template<typename Socket>
class TestConnection:
    public nx::network::stun::AbstractServerConnection,
    public nx::network::aio::BasicPollable
{
public:
    virtual void sendMessage(
        nx::network::stun::Message /*message*/,
        std::function<void(SystemError::ErrorCode)> /*handler*/ = nullptr) override
    {
    }

    virtual nx::network::TransportProtocol transportProtocol() const override
    {
        return SocketTypeToProtocolType<Socket>::value;
    }

    virtual nx::network::SocketAddress getSourceAddress() const override
    {
        return nx::network::SocketAddress();
    }

    virtual void addOnConnectionCloseHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_connectionCloseHandler.swap(handler);
    }

    virtual nx::network::AbstractCommunicatingSocket* socket() override
    {
        return &m_socket;
    }

    virtual void close() override
    {
    }

    virtual void setInactivityTimeout(
        std::optional<std::chrono::milliseconds> value) override
    {
        m_inactivityTimeout = value;
    }

    std::optional<std::chrono::milliseconds> inactivityTimeout() const
    {
        return m_inactivityTimeout;
    }

    void reportConnectionClosure()
    {
        post(
            [this]()
            {
                if (m_connectionCloseHandler)
                    nx::utils::swapAndCall(m_connectionCloseHandler, SystemError::connectionReset);
            });
    }

private:
    Socket m_socket;
    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectionCloseHandler;
};

//-------------------------------------------------------------------------------------------------

class TestTcpConnection:
    public nx::network::stun::ServerConnection
{
    using base_type = nx::network::stun::ServerConnection;

public:
    TestTcpConnection();

    virtual void sendMessage(
        nx::network::stun::Message /*message*/,
        std::function<void(SystemError::ErrorCode)> /*handler*/ = nullptr) override;

    virtual nx::network::SocketAddress getSourceAddress() const override;

    virtual void setInactivityTimeout(
        std::optional<std::chrono::milliseconds> value) override;

    std::optional<std::chrono::milliseconds> inactivityTimeout() const;

private:
    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
};

} // namespace test
} // namespace hpm
} // namespace nx
