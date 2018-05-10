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
    std::unique_ptr<AbstractStreamSocket> delegate,
    bool isServerSide)
    :
    base_type(delegate.get()),
    m_delegate(std::move(delegate)),
    m_socketToPipelineAdapter(m_delegate.get()),
    m_proxyConverter(nullptr)
{
    if (isServerSide)
        m_sslPipeline = std::make_unique<ssl::AcceptingPipeline>();
    else
        m_sslPipeline = std::make_unique<ssl::ConnectingPipeline>();

    m_proxyConverter.setDelegate(m_sslPipeline.get());
    m_asyncTransformingChannel = std::make_unique<aio::StreamTransformingAsyncChannel>(
        aio::makeAsyncChannelAdapter(m_delegate.get()),
        &m_proxyConverter);

    base_type::bindToAioThread(m_delegate->getAioThread());
}

StreamSocket::~StreamSocket()
{
}

void StreamSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void StreamSocket::pleaseStopSync(bool checkForLocks)
{
    if (!m_delegate)
        return;

    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
    }
    else
    {
        std::promise<void> stopped;
        pleaseStop(
            [&stopped]()
            {
                stopped.set_value();
            });
        stopped.get_future().wait();
    }
}

void StreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_asyncTransformingChannel->bindToAioThread(aioThread);
    m_delegate->bindToAioThread(aioThread);
}

bool StreamSocket::connect(
    const SocketAddress& remoteSocketAddress,
    std::chrono::milliseconds timeout)
{
    if (!base_type::connect(remoteSocketAddress, timeout))
        return false;

    switchToSyncModeIfNeeded();
    return m_sslPipeline->performHandshake();
}

void StreamSocket::connectAsync(
    const SocketAddress& address,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    base_type::connectAsync(
        address,
        [this, handler = std::move(handler)](
            SystemError::ErrorCode connectResultCode) mutable
        {
            if (connectResultCode != SystemError::noError)
                return handler(connectResultCode);

            handshakeAsync(std::move(handler));
        });
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

void StreamSocket::handshakeAsync(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    switchToAsyncModeIfNeeded();

    m_asyncTransformingChannel->sendAsync(
        m_emptyBuffer,
        [this, handler = std::move(handler)](
            SystemError::ErrorCode systemErrorCode,
            std::size_t /*bytesSent*/)
        {
            handler(systemErrorCode);
        });
}

void StreamSocket::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    // Performing handshake (part of connect) and cancellation of connect has been requested?
    if (eventType == aio::EventType::etWrite && !m_sslPipeline->isHandshakeCompleted())
    {
        // Then we cancel all I/O since handshake invokes both send & recv.
        eventType = aio::EventType::etNone;
        m_asyncTransformingChannel->cancelPostedCallsSync();
    }

    m_delegate->cancelIOSync(eventType);
    m_asyncTransformingChannel->cancelIOSync(eventType);
}

void StreamSocket::stopWhileInAioThread()
{
    m_asyncTransformingChannel.reset();
    m_delegate.reset();
}

void StreamSocket::switchToSyncModeIfNeeded()
{
    m_sslPipeline->setInput(&m_socketToPipelineAdapter);
    m_sslPipeline->setOutput(&m_socketToPipelineAdapter);
}

void StreamSocket::switchToAsyncModeIfNeeded()
{
    m_proxyConverter.setDelegate(m_sslPipeline.get());
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

//-------------------------------------------------------------------------------------------------

ClientStreamSocket::ClientStreamSocket(
    std::unique_ptr<AbstractStreamSocket> delegate)
    :
    base_type(std::move(delegate), false)
{
}

ServerSideStreamSocket::ServerSideStreamSocket(
    std::unique_ptr<AbstractStreamSocket> delegate)
    :
    base_type(std::move(delegate), true)
{
}

} // namespace ssl
} // namespace network
} // namespace nx
