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
    public stun::AbstractServerConnection,
    public nx::network::aio::BasicPollable
{
public:
    virtual void sendMessage(
        nx::stun::Message /*message*/,
        std::function<void(SystemError::ErrorCode)> /*handler*/ = nullptr) override
    {
    }

    virtual nx::network::TransportProtocol transportProtocol() const override
    {
        return SocketTypeToProtocolType<Socket>::value;
    }

    virtual SocketAddress getSourceAddress() const override
    {
        return SocketAddress();
    }

    virtual void addOnConnectionCloseHandler(
        nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_connectionCloseHandler.swap(handler);
    }

    virtual AbstractCommunicatingSocket* socket() override
    {
        return &m_socket;
    }

    virtual void close() override
    {
    }

    virtual void setInactivityTimeout(
        boost::optional<std::chrono::milliseconds> value) override
    {
        m_inactivityTimeout = value;
    }

    boost::optional<std::chrono::milliseconds> inactivityTimeout() const
    {
        return m_inactivityTimeout;
    }

    void reportConnectionClosure()
    {
        post(
            [this]()
            {
                if (m_connectionCloseHandler)
                    nx::utils::swapAndCall(m_connectionCloseHandler);
            });
    }

private:
    Socket m_socket;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    nx::utils::MoveOnlyFunc<void()> m_connectionCloseHandler;
};

//-------------------------------------------------------------------------------------------------

class TestTcpConnection:
    public stun::ServerConnection
{
    using base_type = stun::ServerConnection;

public:
    TestTcpConnection();

    virtual void sendMessage(
        nx::stun::Message /*message*/,
        std::function<void(SystemError::ErrorCode)> /*handler*/ = nullptr) override;

    virtual SocketAddress getSourceAddress() const override;

    virtual void setInactivityTimeout(
        boost::optional<std::chrono::milliseconds> value) override;

    boost::optional<std::chrono::milliseconds> inactivityTimeout() const;

private:
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
};

} // namespace test
} // namespace hpm
} // namespace nx
