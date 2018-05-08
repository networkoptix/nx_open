#include "ssl_stream_server_socket.h"

#include "encryption_detecting_stream_socket.h"
#include "ssl_stream_socket.h"

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
            std::unique_ptr<AbstractStreamSocket> streamSocket) mutable
        {
            onAcceptCompletion(
                std::move(handler),
                sysErrorCode,
                std::move(streamSocket));
        });
}

AbstractStreamSocket* StreamServerSocket::accept()
{
    AbstractStreamSocket* accepted = base_type::accept();
    if (accepted)
        accepted = new StreamSocket(std::unique_ptr<AbstractStreamSocket>(accepted), true);
    return accepted;
}

void StreamServerSocket::onAcceptCompletion(
    AcceptCompletionHandler handler,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    if (streamSocket)
    {
        if (m_encryptionUse == EncryptionUse::always)
        {
            streamSocket = std::make_unique<ServerSideStreamSocket>(
                std::move(streamSocket));
        }
        else // if (m_encryptionUse == EncryptionUse::autoDetectByReceivedData)
        {
            streamSocket = std::make_unique<EncryptionDetectingStreamSocket>(
                std::move(streamSocket));
        }
    }

    handler(sysErrorCode, std::move(streamSocket));
}

} // namespace ssl
} // namespace network
} // namespace nx
