#pragma once

#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

enum class EncryptionUse
{
    always,
    autoDetectByReceivedData
};

class NX_NETWORK_API StreamServerSocket:
    public StreamServerSocketDelegate
{
    using base_type = StreamServerSocketDelegate;

public:
    StreamServerSocket(
        std::unique_ptr<AbstractStreamServerSocket> delegatee,
        EncryptionUse encryptionUse);

    virtual void acceptAsync(AcceptCompletionHandler handler) override;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_delegatee;

    void onAcceptCompletion(
        AcceptCompletionHandler handler,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> streamSocket);
};

} // namespace ssl
} // namespace network
} // namespace nx
