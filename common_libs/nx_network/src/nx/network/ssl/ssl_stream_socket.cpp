#include "ssl_stream_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamSocket::StreamSocket(
    std::unique_ptr<AbstractStreamSocket> wrappedSocket,
    bool /*isServerSide*/,
    EncryptionUse /*encryptionUse*/)
    :
    base_type(std::move(wrappedSocket))
{
}

StreamSocket::~StreamSocket()
{
}

} // namespace ssl
} // namespace network
} // namespace nx
