#include "ssl_stream_socket.h"

#include "../aio/async_channel_adapter.h"

namespace nx {
namespace network {
namespace ssl {

StreamSocketToTwoWayPipelineAdapter::StreamSocketToTwoWayPipelineAdapter(
    AbstractStreamSocket* streamSocket)
    :
    m_streamSocket(streamSocket)
{
}

StreamSocketToTwoWayPipelineAdapter::~StreamSocketToTwoWayPipelineAdapter()
{
}

int StreamSocketToTwoWayPipelineAdapter::read(void* data, size_t count)
{
    const int bytesRead = m_streamSocket->recv(data, static_cast<int>(count));
    return bytesTransferredToPipelineReturnCode(bytesRead);
}

int StreamSocketToTwoWayPipelineAdapter::write(const void* data, size_t count)
{
    const int bytesWritten = m_streamSocket->send(data, static_cast<int>(count));
    return bytesTransferredToPipelineReturnCode(bytesWritten);
}

int StreamSocketToTwoWayPipelineAdapter::bytesTransferredToPipelineReturnCode(
    int bytesTransferred)
{
    if (bytesTransferred >= 0)
        return bytesTransferred;

    switch (SystemError::getLastOSErrorCode())
    {
        case SystemError::timedOut:
        case SystemError::wouldBlock:
            return utils::bstream::StreamIoError::wouldBlock;

        default:
            return utils::bstream::StreamIoError::osError;
    }
}

//-------------------------------------------------------------------------------------------------

StreamSocket::StreamSocket(
    std::unique_ptr<AbstractStreamSocket> delegatee,
    bool isServerSide)
    :
    base_type(delegatee.get()),
    m_delegatee(std::move(delegatee)),
    m_socketToPipelineAdapter(m_delegatee.get()),
    m_proxyConverter(nullptr)
{
    if (isServerSide)
        m_sslPipeline = std::make_unique<ssl::AcceptingPipeline>();
    else
        m_sslPipeline = std::make_unique<ssl::ConnectingPipeline>();

    m_proxyConverter.setDelegatee(m_sslPipeline.get());
    m_asyncTransformingChannel = std::make_unique<aio::StreamTransformingAsyncChannel>(
        aio::makeAsyncChannelAdapter(m_delegatee.get()),
        &m_proxyConverter);

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
    m_asyncTransformingChannel->bindToAioThread(aioThread);
    m_delegatee->bindToAioThread(aioThread);
}

int StreamSocket::recv(void* buffer, unsigned int bufferLen, int /*flags*/)
{
    switchToSyncModeIfNeeded();
    return m_sslPipeline->read(buffer, bufferLen);
}

int StreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    switchToSyncModeIfNeeded();
    return m_sslPipeline->write(buffer, bufferLen);
}

void StreamSocket::readSomeAsync(
    nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    switchToAsyncModeIfNeeded();
    m_asyncTransformingChannel->readSomeAsync(buffer, std::move(handler));
}

void StreamSocket::sendAsync(
    const nx::Buffer& buffer,
    IoCompletionHandler handler)
{
    switchToAsyncModeIfNeeded();
    m_asyncTransformingChannel->sendAsync(buffer, std::move(handler));
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
    m_asyncTransformingChannel->cancelIOSync(eventType);
}

void StreamSocket::stopWhileInAioThread()
{
    m_asyncTransformingChannel.reset();
    m_delegatee.reset();
}

void StreamSocket::switchToSyncModeIfNeeded()
{
    m_sslPipeline->setInput(&m_socketToPipelineAdapter);
    m_sslPipeline->setOutput(&m_socketToPipelineAdapter);
}

void StreamSocket::switchToAsyncModeIfNeeded()
{
    m_proxyConverter.setDelegatee(m_sslPipeline.get());
}

} // namespace ssl
} // namespace network
} // namespace nx
