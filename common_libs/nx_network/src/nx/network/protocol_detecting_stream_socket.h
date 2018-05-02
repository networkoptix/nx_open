#pragma once

#include <list>
#include <memory>

#include "aio/protocol_detecting_async_channel.h"
#include "socket_delegate.h"

namespace nx {
namespace network {

/**
 * Can be used as a server-side connection only.
 * Protocol selection on client side MUST be done explicitly by using appropriate socket class.
 */
class NX_NETWORK_API ProtocolDetectingStreamSocket:
    public aio::BaseProtocolDetectingAsyncChannel<StreamSocketDelegate, AbstractStreamSocket>
{
    using base_type =
        aio::BaseProtocolDetectingAsyncChannel<StreamSocketDelegate, AbstractStreamSocket>;

public:
    ProtocolDetectingStreamSocket(std::unique_ptr<AbstractStreamSocket> source);

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override;

    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;

    virtual int send(const void* buffer, unsigned int bufferLen) override;

protected:
    virtual std::unique_ptr<AbstractStreamSocket> installProtocolHandler(
        ProtocolProcessorFactoryFunc& factoryFunc,
        std::unique_ptr<AbstractStreamSocket> dataSource,
        nx::Buffer readDataCache) override;
};

} // namespace network
} // namespace nx
