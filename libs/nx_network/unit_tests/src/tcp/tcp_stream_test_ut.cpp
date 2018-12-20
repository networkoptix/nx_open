#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/test_support/stream_socket_acceptance_tests.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {
namespace test {

//-------------------------------------------------------------------------------------------------
// Ipv4

struct TcpSocketIpv4TypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = TCPServerSocket;
};

INSTANTIATE_TYPED_TEST_CASE_P(TcpSocketIpv4, StreamSocketAcceptance, TcpSocketIpv4TypeSet);

//-------------------------------------------------------------------------------------------------
// Ipv6

class TCPSocketV6:
    public TCPSocket
{
public:
    TCPSocketV6():
        TCPSocket(AF_INET6)
    {
    }
};

class TCPServerSocketV6:
    public TCPServerSocket
{
public:
    TCPServerSocketV6():
        TCPServerSocket(AF_INET6)
    {
        NX_GTEST_ASSERT_TRUE(setIpv6Only(true));
    }
};

struct TcpSocketIpv6TypeSet
{
    using ClientSocket = TCPSocketV6;
    using ServerSocket = TCPServerSocketV6;
};

INSTANTIATE_TYPED_TEST_CASE_P(TcpSocketIpv6, StreamSocketAcceptance, TcpSocketIpv6TypeSet);

//-------------------------------------------------------------------------------------------------
// Ipv6 dual stack mode. Client socket is ipv6, server - ipv4.

struct TcpSocketIpv6DualStackModeTypeSet
{
    using ClientSocket = TCPSocketV6;
    using ServerSocket = TCPServerSocket;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    TcpSocketIpv6DualStackMode,
    StreamSocketAcceptance,
    TcpSocketIpv6DualStackModeTypeSet);

} // namespace test
} // namespace network
} // namespace nx
