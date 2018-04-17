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
    const int bytesRead = m_streamSocket->recv(
        data, static_cast<int>(count), getFlagsForCurrentThread());
    return bytesTransferredToPipelineReturnCode(bytesRead);
}

int StreamSocketToTwoWayPipelineAdapter::getFlagsForCurrentThread() const
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_threadIdToFlags.find(std::this_thread::get_id());
    return it != m_threadIdToFlags.end() ? it->second : 0;
}

int StreamSocketToTwoWayPipelineAdapter::write(const void* data, size_t count)
{
    const int bytesWritten = m_streamSocket->send(data, static_cast<int>(count));
    return bytesTransferredToPipelineReturnCode(bytesWritten);
}

void StreamSocketToTwoWayPipelineAdapter::setFlagsForCallsInThread(
    std::thread::id threadId,
    int flags)
{
    QnMutexLocker lock(&m_mutex);
    if (flags == 0)
        m_threadIdToFlags.erase(threadId);
    else
        m_threadIdToFlags[threadId] = flags;
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

int StreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    switchToSyncModeIfNeeded();
    // TODO: #ak setFlagsForCallsInThread is a hack. Have to pass flags
    // via m_sslPipeline->read call or get rid of flags in StreamSocket::recv at all.
    if (flags != 0)
        m_socketToPipelineAdapter.setFlagsForCallsInThread(std::this_thread::get_id(), flags);
    const int result = m_sslPipeline->read(buffer, bufferLen);
    if (flags != 0)
        m_socketToPipelineAdapter.setFlagsForCallsInThread(std::this_thread::get_id(), 0);
    if (result >= 0)
        return result;
    handleSslError(result);
    return -1;
}

int StreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    switchToSyncModeIfNeeded();
    const int result = m_sslPipeline->write(buffer, bufferLen);
    if (result >= 0)
        return result;
    handleSslError(result);
    return -1;
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

void StreamSocket::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_delegatee->cancelIOSync(eventType);
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

void StreamSocket::handleSslError(int sslPipelineResultCode)
{
    if (sslPipelineResultCode == nx::utils::bstream::StreamIoError::wouldBlock &&
        SystemError::getLastOSErrorCode() != SystemError::timedOut) //< TODO: #ak Come up with a better way to pass os error.
    {
        SystemError::setLastErrorCode(SystemError::wouldBlock);
    }
    else if (sslPipelineResultCode == nx::utils::bstream::StreamIoError::nonRecoverableError)
    {
        SystemError::setLastErrorCode(SystemError::invalidData);
    }
}

} // namespace ssl
} // namespace network
} // namespace nx
