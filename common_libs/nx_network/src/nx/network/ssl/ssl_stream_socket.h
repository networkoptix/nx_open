#pragma once

#include <memory>

#include "ssl_pipeline.h"
#include "../aio/stream_transforming_async_channel.h"
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
        std::unique_ptr<AbstractStreamSocket> delegatee,
        bool isServerSide,  // TODO: #ak Get rid of this one.
        EncryptionUse encryptionUse = EncryptionUse::autoDetectByReceivedData);

    virtual ~StreamSocket() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_delegatee;
};

} // namespace ssl
} // namespace network
} // namespace nx
