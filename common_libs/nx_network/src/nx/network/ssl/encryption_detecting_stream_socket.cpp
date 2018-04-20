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

std::unique_ptr<AbstractStreamSocket> EncryptionDetectingStreamSocket::createSslSocket(
    std::unique_ptr<AbstractStreamSocket> rawDataSource)
{
    return std::make_unique<ssl::StreamSocket>(std::move(rawDataSource), true);
}

} // namespace ssl
} // namespace network
} // namespace nx
