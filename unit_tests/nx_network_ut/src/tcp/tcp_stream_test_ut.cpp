#include <nx/network/system_socket.h>

#include <nx/network/test_support/stream_socket_acceptance_tests.h>

namespace nx {
namespace network {
namespace test {

struct TcpSocketIpv4TypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = TCPServerSocket;
};

INSTANTIATE_TYPED_TEST_CASE_P(TcpSocketIpv4Stream, StreamSocketAcceptance, TcpSocketIpv4TypeSet);

//-------------------------------------------------------------------------------------------------

namespace {

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
    }
};

struct TcpSocketIpv6TypeSet
{
    using ClientSocket = TCPSocketV6;
    using ServerSocket = TCPServerSocketV6;
};

} // namespace

INSTANTIATE_TYPED_TEST_CASE_P(TcpSocketIpv6TypeSet, StreamSocketAcceptance, TcpSocketIpv6TypeSet);

} // namespace test
} // namespace network
} // namespace nx
