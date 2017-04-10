#include "ssl_stream_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamSocket::StreamSocket(
    AbstractStreamSocket* wrappedSocket,
    bool /*isServerSide*/,
    EncryptionUse /*encryptionUse*/)
    :
    base_type(wrappedSocket)
{
}

StreamSocket::~StreamSocket()
{
}

} // namespace ssl
} // namespace network
} // namespace nx
