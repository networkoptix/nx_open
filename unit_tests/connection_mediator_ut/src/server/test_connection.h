#pragma once

#include <nx/network/stun/abstract_server_connection.h>
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

template<typename Socket>
class TestConnection:
    public stun::AbstractServerConnection
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

    virtual void addOnConnectionCloseHandler(nx::utils::MoveOnlyFunc<void()> /*handler*/) override
    {
    }

    virtual AbstractCommunicatingSocket* socket() override
    {
        return &m_socket;
    }

    virtual void close() override
    {
    }

private:
    Socket m_socket;
};

} // namespace test
} // namespace hpm
} // namespace nx
