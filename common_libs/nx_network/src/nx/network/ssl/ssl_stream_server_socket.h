#pragma once

#include "ssl_stream_socket.h"
#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

class NX_NETWORK_API StreamServerSocket:
    public StreamServerSocketDelegate
{
    using base_type = StreamServerSocketDelegate;

public:
    StreamServerSocket(
        AbstractStreamServerSocket* delegateSocket,
        EncryptionUse encryptionUse);

    virtual void acceptAsync(AcceptCompletionHandler handler) override;

private:
    EncryptionUse m_encryptionUse;

    void onAcceptCompletion(
        AcceptCompletionHandler handler,
        SystemError::ErrorCode sysErrorCode,
        AbstractStreamSocket* streamSocket);
};

} // namespace ssl
} // namespace network
} // namespace nx
