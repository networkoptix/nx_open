#include "protocol_detecting_stream_socket.h"

#include <functional>
#include <memory>

#include "aio/async_channel_adapter.h"
#include "buffered_stream_socket.h"

namespace nx {
namespace network {

ProtocolDetectingStreamSocket::ProtocolDetectingStreamSocket(
    std::unique_ptr<AbstractStreamSocket> source)
    :
    base_type(source.get())
{
    setDataSource(std::move(source));
}

bool ProtocolDetectingStreamSocket::connect(
    const SocketAddress& /*remoteSocketAddress*/,
    std::chrono::milliseconds /*timeout*/)
{
    NX_ASSERT(false);
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

void ProtocolDetectingStreamSocket::connectAsync(
    const SocketAddress& /*address*/,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    NX_ASSERT(false);
    post(
        [handler = std::move(handler)]()
        {
            handler(SystemError::notImplemented);
        });
}

int ProtocolDetectingStreamSocket::recv(
    void* buffer, unsigned int bufferLen, int flags)
{
    while (!isProtocolDetected())
    {
        std::array<char, 1024> buf;
        int bytesRead = actualDataChannel().recv(buf.data(), buf.size(), 0);
        if (bytesRead <= 0)
            return bytesRead;

        analyzeMoreData(nx::Buffer(buf.data(), bytesRead));
    }

    return actualDataChannel().recv(buffer, bufferLen, flags);
}

int ProtocolDetectingStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    if (!isProtocolDetected())
    {
        SystemError::setLastErrorCode(SystemError::invalidData);
        return -1;
    }

    return actualDataChannel().send(buffer, bufferLen);
}

std::unique_ptr<AbstractStreamSocket> ProtocolDetectingStreamSocket::installProtocolHandler(
    ProtocolProcessorFactoryFunc& factoryFunc,
    std::unique_ptr<AbstractStreamSocket> dataSource,
    nx::Buffer readDataCache)
{
    return factoryFunc(std::make_unique<BufferedStreamSocket>(
        std::move(dataSource),
        std::move(readDataCache)));
}

} // namespace network
} // namespace nx
