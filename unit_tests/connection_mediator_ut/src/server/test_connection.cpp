#include "test_connection.h"

#include <nx/network/stun/message_dispatcher.h>

namespace nx {
namespace hpm {
namespace test {

// Introducing this variable just to simplify tests that do not need dispatcher explicitely.
static nx::network::stun::MessageDispatcher s_messageDispatcher;

TestTcpConnection::TestTcpConnection():
    base_type(
        nullptr,
        std::make_unique<nx::network::TCPSocket>(AF_INET),
        s_messageDispatcher)
{
}

void TestTcpConnection::sendMessage(
    nx::network::stun::Message /*message*/,
    std::function<void(SystemError::ErrorCode)> /*handler*/)
{
}

nx::network::SocketAddress TestTcpConnection::getSourceAddress() const
{
    return nx::network::SocketAddress();
}

void TestTcpConnection::setInactivityTimeout(
    boost::optional<std::chrono::milliseconds> value)
{
    m_inactivityTimeout = value;
}

boost::optional<std::chrono::milliseconds> TestTcpConnection::inactivityTimeout() const
{
    return m_inactivityTimeout;
}

} // namespace test
} // namespace hpm
} // namespace nx
