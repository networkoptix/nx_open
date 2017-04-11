#include "ssl_stream_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamSocket::StreamSocket(
    std::unique_ptr<AbstractStreamSocket> delegatee,
    bool /*isServerSide*/,
    EncryptionUse /*encryptionUse*/)
    :
    base_type(delegatee.get()),
    m_delegatee(std::move(delegatee))
{
}

StreamSocket::~StreamSocket()
{
}

} // namespace ssl
} // namespace network
} // namespace nx
