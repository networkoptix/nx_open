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
            if (streamSocket)
                streamSocket = createSocketWrapper(std::move(streamSocket));

            handler(sysErrorCode, std::move(streamSocket));
        });
}

AbstractStreamSocket* StreamServerSocket::accept()
{
    std::unique_ptr<AbstractStreamSocket> accepted(base_type::accept());
    if (!accepted)
        return nullptr;

    return createSocketWrapper(std::move(accepted)).release();
}

std::unique_ptr<AbstractStreamSocket> StreamServerSocket::createSocketWrapper(
    std::unique_ptr<AbstractStreamSocket> delegate)
{
    switch (m_encryptionUse)
    {
        case EncryptionUse::always:
            return std::make_unique<ServerSideStreamSocket>(std::move(delegate));

        case EncryptionUse::autoDetectByReceivedData:
            return std::make_unique<EncryptionDetectingStreamSocket>(std::move(delegate));

        default:
            NX_ASSERT(false);
            return nullptr;
    }
}

} // namespace ssl
} // namespace network
} // namespace nx
