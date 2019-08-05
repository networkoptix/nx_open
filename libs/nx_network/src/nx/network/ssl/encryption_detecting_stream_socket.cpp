#include "encryption_detecting_stream_socket.h"

#include <memory>

#include "ssl_stream_socket.h"
#include "../aio/async_channel_adapter.h"

namespace nx {
namespace network {
namespace ssl {

EncryptionDetectingStreamSocket::EncryptionDetectingStreamSocket(
    std::unique_ptr<AbstractStreamSocket> source)
    :
    base_type(std::move(source))
{
    using namespace std::placeholders;

    registerProtocol(
        std::make_unique<FixedProtocolPrefixRule>(std::string{(char)0x80}),
        std::bind(&EncryptionDetectingStreamSocket::createSslSocket, this, _1));

    registerProtocol(
        std::make_unique<FixedProtocolPrefixRule>(std::string{(char)0x16, (char)0x03}),
        std::bind(&EncryptionDetectingStreamSocket::createSslSocket, this, _1));
}

bool EncryptionDetectingStreamSocket::isEncryptionEnabled() const
{
    return m_sslUsed;
}

void EncryptionDetectingStreamSocket::handshakeAsync(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    using namespace std::placeholders;

    m_handshakeCompletionUserHandler = std::move(handler);
    detectProtocol(std::bind(
        &EncryptionDetectingStreamSocket::proceedWithSslHandshakeIfNeeded, this, _1));
}

std::string EncryptionDetectingStreamSocket::serverName() const
{
    if (!m_sslUsed)
        return std::string();

    return static_cast<const ssl::StreamSocket&>(actualDataChannel()).serverName();
}

std::unique_ptr<AbstractStreamSocket> EncryptionDetectingStreamSocket::createSslSocket(
    std::unique_ptr<AbstractStreamSocket> rawDataSource)
{
    m_sslUsed = true;
    return std::make_unique<ssl::StreamSocket>(std::move(rawDataSource), true);
}

void EncryptionDetectingStreamSocket::proceedWithSslHandshakeIfNeeded(
    SystemError::ErrorCode protocolDetectionResult)
{
    if (protocolDetectionResult != SystemError::noError)
    {
        nx::utils::swapAndCall(m_handshakeCompletionUserHandler, protocolDetectionResult);
        return;
    }

    if (!m_sslUsed)
    {
        nx::utils::swapAndCall(m_handshakeCompletionUserHandler, SystemError::noError);
        return;
    }

    static_cast<ssl::StreamSocket&>(actualDataChannel()).handshakeAsync(
        std::exchange(m_handshakeCompletionUserHandler, nullptr));
}

} // namespace ssl
} // namespace network
} // namespace nx
