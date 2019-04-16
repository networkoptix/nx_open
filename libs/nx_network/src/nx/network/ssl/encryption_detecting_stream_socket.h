#pragma once

#include <atomic>
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
    public ProtocolDetectingStreamSocket<AbstractEncryptedStreamSocket>
{
    using base_type = ProtocolDetectingStreamSocket<AbstractEncryptedStreamSocket>;

public:
    EncryptionDetectingStreamSocket(std::unique_ptr<AbstractStreamSocket> source);

    virtual bool isEncryptionEnabled() const override;

    /**
     * Reads connection until protocol is detected.
     * If SSL is used, then performs SSL handshake.
     */
    virtual void handshakeAsync(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual std::string serverName() const override;

private:
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_handshakeCompletionUserHandler;
    std::atomic<bool> m_sslUsed = false;

    std::unique_ptr<AbstractStreamSocket> createSslSocket(
        std::unique_ptr<AbstractStreamSocket> /*rawDataSource*/);

    void proceedWithSslHandshakeIfNeeded(
        SystemError::ErrorCode systemErrorCode);
};

} // namespace ssl
} // namespace network
} // namespace nx
