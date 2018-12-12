#include "buffered_async_channel.h"

namespace nx {
namespace network {
namespace aio {

BufferedAsyncChannel::BufferedAsyncChannel(
    std::unique_ptr<AbstractAsyncChannel> source,
    nx::Buffer buffer)
    :
    base_type(std::move(source)),
    m_internalRecvBuffer(std::move(buffer))
{
}

void BufferedAsyncChannel::readSomeAsync(
    nx::Buffer* const buf,
    IoCompletionHandler handler)
{
    if (m_internalRecvBuffer.isEmpty())
        return base_type::readSomeAsync(buf, std::move(handler));

    post(
        [this, buf, handler = std::move(handler)]()
        {
            // Draining internal buffer.
            auto recvSize = std::min(m_internalRecvBuffer.size(), buf->capacity() - buf->size());

            auto oldSize = buf->size();
            buf->resize(oldSize + recvSize);

            std::memcpy(buf->data() + oldSize, m_internalRecvBuffer.data(), recvSize);
            m_internalRecvBuffer = m_internalRecvBuffer.mid(recvSize);

            handler(SystemError::noError, (size_t)recvSize);
        });
}

void BufferedAsyncChannel::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    // TODO: #ak Have to cancel post done in BufferedAsyncChannel::readSomeAsync.
    base_type::cancelIoInAioThread(eventType);
}

} // namespace aio
} // namespace network
} // namespace nx
