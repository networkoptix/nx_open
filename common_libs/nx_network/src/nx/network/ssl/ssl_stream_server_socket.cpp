#include "ssl_stream_server_socket.h"

#include "ssl_stream_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamServerSocket::StreamServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> delegatee,
    EncryptionUse /*encryptionUse*/)
    :
    base_type(delegatee.get()),
    m_delegatee(std::move(delegatee))
{
}

void StreamServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    return m_delegatee->acceptAsync(
        [this, handler = std::move(handler)](
            SystemError::ErrorCode sysErrorCode,
            std::unique_ptr<AbstractStreamSocket> streamSocket) mutable
        {
            onAcceptCompletion(
                std::move(handler),
                sysErrorCode,
                std::move(streamSocket));
        });
}

void StreamServerSocket::onAcceptCompletion(
    AcceptCompletionHandler handler,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    if (streamSocket)
        streamSocket = std::make_unique<StreamSocket>(std::move(streamSocket), true);

    handler(sysErrorCode, std::move(streamSocket));
}

} // namespace ssl
} // namespace network
} // namespace nx
