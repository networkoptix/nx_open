#pragma once

#include <memory>

#include "../socket_delegate.h"

namespace nx {
namespace network {
namespace ssl {

enum class EncryptionUse
{
    always,
    autoDetectByReceivedData
};

class NX_NETWORK_API StreamSocket:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    // TODO: #ak encryptionUse here looks strange.
    StreamSocket(
        std::unique_ptr<AbstractStreamSocket> wrappedSocket,
        bool isServerSide,  // TODO: #ak Get rid of this one.
        EncryptionUse encryptionUse = EncryptionUse::autoDetectByReceivedData);

    virtual ~StreamSocket() override;
};

} // namespace ssl
} // namespace network
} // namespace nx
