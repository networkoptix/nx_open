#include "ssl_stream_socket.h"

#include "../aio/async_channel_adapter.h"

namespace nx {
namespace network {
namespace ssl {

StreamSocket::StreamSocket(
    std::unique_ptr<AbstractStreamSocket> delegatee,
    bool isServerSide,
    EncryptionUse /*encryptionUse*/)
    :
    base_type(delegatee.get()),
    m_delegatee(std::move(delegatee))
{
    std::unique_ptr<ssl::Pipeline> sslPipeline;
    if (isServerSide)
        sslPipeline = std::make_unique<ssl::AcceptingPipeline>();
    else
        sslPipeline = std::make_unique<ssl::ConnectingPipeline>();

    m_trasformingChannel = std::make_unique<aio::StreamTransformingAsyncChannel>(
        aio::makeAsyncChannelAdapter(m_delegatee.get()),
        std::move(sslPipeline));

    base_type::bindToAioThread(m_delegatee->getAioThread());
}

StreamSocket::~StreamSocket()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void StreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_trasformingChannel->bindToAioThread(aioThread);
    m_delegatee->bindToAioThread(aioThread);
}

void StreamSocket::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_trasformingChannel->readSomeAsync(buffer, std::move(handler));
}

void StreamSocket::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    m_trasformingChannel->sendAsync(buffer, std::move(handler));
}

void StreamSocket::cancelIOAsync(
    nx::network::aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, eventType, handler = std::move(handler)]()
        {
            cancelIOSync(eventType);
            handler();
        });
}

void StreamSocket::cancelIOSync(nx::network::aio::EventType eventType)
{
    m_trasformingChannel->cancelIOSync(eventType);
}

void StreamSocket::stopWhileInAioThread()
{
    m_trasformingChannel.reset();
    m_delegatee.reset();
}

} // namespace ssl
} // namespace network
} // namespace nx
