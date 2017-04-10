#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

namespace nx {
namespace network {
namespace test {

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslFreeTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(new TCPServerSocket(AF_INET), true); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslEnabledTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(new TCPServerSocket(AF_INET), true); },
    [](){ return std::make_unique<deprecated::SslSocket>(new TCPSocket(AF_INET), false); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslEnforcedTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(new TCPServerSocket(AF_INET), false); },
    [](){ return std::make_unique<deprecated::SslSocket>(new TCPSocket(AF_INET), false); });

} // namespace test
} // namespace network
} // namespace nx
