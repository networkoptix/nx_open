#pragma once

#include <functional>
#include <list>
#include <memory>

#include "aio/protocol_detecting_async_channel.h"
#include "aio/async_channel_adapter.h"
#include "buffered_stream_socket.h"
#include "socket_delegate.h"

namespace nx {
namespace network {

/**
 * Can be used as a server-side connection only.
 * Protocol selection on client side MUST be done explicitly by using appropriate socket class.
 */
template <typename SocketInterface>
class ProtocolDetectingStreamSocket:
    public aio::BaseProtocolDetectingAsyncChannel<
        CustomStreamSocketDelegate<SocketInterface, AbstractStreamSocket>,
        AbstractStreamSocket>
{
    using base_type =
        aio::BaseProtocolDetectingAsyncChannel<
            CustomStreamSocketDelegate<SocketInterface, AbstractStreamSocket>,
            AbstractStreamSocket>;

public:
    ProtocolDetectingStreamSocket(std::unique_ptr<AbstractStreamSocket> source):
        base_type(source.get())
    {
        this->setDataSource(std::move(source));
    }

    virtual bool connect(
        const SocketAddress& /*remoteSocketAddress*/,
        std::chrono::milliseconds /*timeout*/) override
    {
        NX_ASSERT(false);
        SystemError::setLastErrorCode(SystemError::notImplemented);
        return false;
    }

    virtual void connectAsync(
        const SocketAddress& /*address*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        NX_ASSERT(false);
        this->post(
            [handler = std::move(handler)]()
            {
                handler(SystemError::notImplemented);
            });
    }

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override
    {
        while (!this->isProtocolDetected())
        {
            std::array<char, 1024> buf;
            int bytesRead = this->actualDataChannel().recv(buf.data(), (unsigned int) buf.size(), 0);
            if (bytesRead <= 0)
                return bytesRead;

            this->analyzeMoreData(nx::Buffer(buf.data(), bytesRead));
        }

        return this->actualDataChannel().recv(buffer, bufferLen, flags);
    }

    virtual int send(const void* buffer, unsigned int bufferLen) override
    {
        if (!this->isProtocolDetected())
        {
            SystemError::setLastErrorCode(SystemError::invalidData);
            return -1;
        }

        return this->actualDataChannel().send(buffer, bufferLen);
    }

protected:
    virtual std::unique_ptr<AbstractStreamSocket> installProtocolHandler(
        typename base_type::ProtocolProcessorFactoryFunc& factoryFunc,
        std::unique_ptr<AbstractStreamSocket> dataSource,
        nx::Buffer readDataCache) override
    {
        return factoryFunc(std::make_unique<BufferedStreamSocket>(
            std::move(dataSource),
            std::move(readDataCache)));
    }
};

} // namespace network
} // namespace nx
