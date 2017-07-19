#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

namespace nx {
namespace network {
namespace deprecated {
namespace test {

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, DeprecatedSslFreeTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(std::make_unique<TCPServerSocket>(AF_INET), true); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, DeprecatedSslEnabledTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(std::make_unique<TCPServerSocket>(AF_INET), true); },
    [](){ return std::make_unique<deprecated::SslSocket>(std::make_unique<TCPSocket>(AF_INET), false); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, DeprecatedSslEnforcedTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(std::make_unique<TCPServerSocket>(AF_INET), false); },
    [](){ return std::make_unique<deprecated::SslSocket>(std::make_unique<TCPSocket>(AF_INET), false); });

} // namespace test
} // namespace deprecated
} // namespace network
} // namespace nx
