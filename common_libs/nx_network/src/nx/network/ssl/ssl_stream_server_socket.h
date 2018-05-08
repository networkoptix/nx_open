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
        std::unique_ptr<AbstractStreamServerSocket> delegate,
        EncryptionUse encryptionUse);

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual AbstractStreamSocket* accept() override;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_delegate;
    EncryptionUse m_encryptionUse;

    std::unique_ptr<AbstractStreamSocket> createSocketWrapper(
        std::unique_ptr<AbstractStreamSocket> delegate);
};

} // namespace ssl
} // namespace network
} // namespace nx
