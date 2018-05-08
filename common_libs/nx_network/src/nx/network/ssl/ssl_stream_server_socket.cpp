#include "ssl_stream_server_socket.h"

#include "encryption_detecting_stream_socket.h"
#include "ssl_stream_socket.h"

namespace nx {
namespace network {
namespace ssl {

StreamServerSocket::StreamServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> delegate,
    EncryptionUse encryptionUse)
    :
    base_type(delegate.get()),
    m_delegate(std::move(delegate)),
    m_encryptionUse(encryptionUse)
{
}

void StreamServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    return m_delegate->acceptAsync(
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
    std::unique_ptr<AbstractStreamSocket> accepted(base_type::accept());
    if (!accepted)
        return nullptr;

    if (m_encryptionUse == EncryptionUse::always)
        return new ServerSideStreamSocket(std::move(accepted));
    else // if (m_encryptionUse == EncryptionUse::autoDetectByReceivedData)
        return new EncryptionDetectingStreamSocket(std::move(accepted));
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
