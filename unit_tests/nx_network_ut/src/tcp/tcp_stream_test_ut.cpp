#include "../stream_socket_ut.h"

namespace nx {
namespace network {
namespace test {

struct TcpSocketTypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = TCPServerSocket;
};

INSTANTIATE_TYPED_TEST_CASE_P(TcpSocketStream, StreamSocket, TcpSocketTypeSet);

} // namespace test
} // namespace network
} // namespace nx
