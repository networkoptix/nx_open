#include <gtest/gtest.h>

#include <utils/common/cpp14.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

namespace nx {
namespace network {
namespace test {

NX_NETWORK_BOTH_SOCKETS_TEST_CASE(
    TEST, SslFreeTcpSockets,
    [](){ return std::make_unique<SslServerSocket>(new TCPServerSocket, true); },
    &std::make_unique<TCPSocket>);

NX_NETWORK_BOTH_SOCKETS_TEST_CASE(
    TEST, SslEnabledTcpSockets,
    [](){ return std::make_unique<SslServerSocket>(new TCPServerSocket, true); },
    [](){ return std::make_unique<SslSocket>(new TCPSocket, false); });

NX_NETWORK_BOTH_SOCKETS_TEST_CASE(
    TEST, SslEnforcedTcpSockets,
    [](){ return std::make_unique<SslServerSocket>(new TCPServerSocket, false); },
    [](){ return std::make_unique<SslSocket>(new TCPSocket, false); });

} // namespace test
} // namespace network
} // namespace nx
