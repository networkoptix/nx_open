#pragma once

#include <memory>

#include "../aio/protocol_detecting_async_channel.h"
#include "../protocol_detecting_stream_socket.h"
#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

class StreamSocket;

/**
 * Can be used as a server-side connection only.
 * Protocol selection on client side MUST be done explicitly by using appropriate socket class.
 */
class NX_NETWORK_API EncryptionDetectingStreamSocket:
    public ProtocolDetectingStreamSocket
{
    using base_type = ProtocolDetectingStreamSocket;

public:
    EncryptionDetectingStreamSocket(std::unique_ptr<AbstractStreamSocket> source);

private:
    std::unique_ptr<AbstractStreamSocket> createSslSocket(
        std::unique_ptr<AbstractStreamSocket> /*rawDataSource*/);
};

} // namespace ssl
} // namespace network
} // namespace nx
