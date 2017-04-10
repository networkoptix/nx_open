#include <gtest/gtest.h>

#include <nx/network/ssl/ssl_stream_server_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace ssl {
namespace test {

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketNotEncryptedConnection,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            new TCPServerSocket(AF_INET),
            EncryptionUse::autoDetectByReceivedData);
    },
    []() { return std::make_unique<TCPSocket>(AF_INET); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketEncryptedConnection,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            new TCPServerSocket(AF_INET),
            EncryptionUse::always);
    },
    []() { return std::make_unique<ssl::StreamSocket>(new TCPSocket(AF_INET), false); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketNotEncryptedConnection2,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            new TCPServerSocket(AF_INET),
            EncryptionUse::autoDetectByReceivedData);
    },
    []() { return std::make_unique<ssl::StreamSocket>(new TCPSocket(AF_INET), false); });

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
