#include "ssl_stream_server_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamServerSocket::StreamServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> delegateSocket,
    EncryptionUse encryptionUse)
    :
    base_type(std::move(delegateSocket)),
    m_encryptionUse(encryptionUse)
{
}

void StreamServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    return m_target->acceptAsync(
        [this, handler = std::move(handler)](
            SystemError::ErrorCode sysErrorCode,
            AbstractStreamSocket* streamSocket) mutable
        {
            onAcceptCompletion(std::move(handler), sysErrorCode, streamSocket);
        });
}

void StreamServerSocket::onAcceptCompletion(
    AcceptCompletionHandler handler,
    SystemError::ErrorCode sysErrorCode,
    AbstractStreamSocket* streamSocket)
{
    handler(sysErrorCode, streamSocket);
}

} // namespace ssl
} // namespace network
} // namespace nx
