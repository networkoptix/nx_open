#include "protocol_detecting_async_channel.h"

#include "buffered_async_channel.h"

namespace nx {
namespace network {
namespace aio {

ProtocolDetectingAsyncChannel::ProtocolDetectingAsyncChannel(
    std::unique_ptr<AbstractAsyncChannel> dataSource)
    :
    base_type(std::move(dataSource))
{
}

std::unique_ptr<AbstractAsyncChannel>
    ProtocolDetectingAsyncChannel::installProtocolHandler(
        ProtocolProcessorFactoryFunc& factoryFunc,
        std::unique_ptr<AbstractAsyncChannel> dataSource,
        nx::Buffer readDataCache)
{
    return factoryFunc(std::make_unique<BufferedAsyncChannel>(
        std::move(dataSource),
        std::move(readDataCache)));
}

void ProtocolDetectingAsyncChannel::stopWhileInAioThread()
{
    base_type::cleanUp();
}

} // namespace aio
} // namespace network
} // namespace nx
