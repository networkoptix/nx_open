#include <nx/network/system_socket.h>

#include <nx/network/test_support/stream_socket_acceptance_tests.h>

namespace nx {
namespace network {
namespace test {

struct TcpSocketTypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = TCPServerSocket;
};

INSTANTIATE_TYPED_TEST_CASE_P(TcpSocketStream, StreamSocketAcceptance, TcpSocketTypeSet);

} // namespace test
} // namespace network
} // namespace nx
