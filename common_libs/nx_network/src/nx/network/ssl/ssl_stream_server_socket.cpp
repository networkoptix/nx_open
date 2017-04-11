#include "ssl_stream_server_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamServerSocket::StreamServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> delegatee,
    EncryptionUse encryptionUse)
    :
    base_type(delegatee.get()),
    m_delegatee(std::move(delegatee)),
    m_encryptionUse(encryptionUse)
{
}

void StreamServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    return m_delegatee->acceptAsync(
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
