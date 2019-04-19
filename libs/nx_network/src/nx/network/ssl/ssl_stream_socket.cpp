#include "ssl_stream_socket.h"

#include "../aio/async_channel_adapter.h"

namespace nx {
namespace network {
namespace ssl {

namespace detail {

StreamSocketToTwoWayPipelineAdapter::StreamSocketToTwoWayPipelineAdapter(
    AbstractStreamSocket* streamSocket,
    aio::StreamTransformingAsyncChannel* asyncSslChannel)
    :
    m_streamSocket(streamSocket),
    m_asyncSslChannel(asyncSslChannel)
{
}

StreamSocketToTwoWayPipelineAdapter::~StreamSocketToTwoWayPipelineAdapter()
{
}

int StreamSocketToTwoWayPipelineAdapter::read(void* data, size_t count)
{
    const int bytesReadFromCache = m_asyncSslChannel->readRawDataFromCache(data, count);
    if (bytesReadFromCache > 0)
        return bytesReadFromCache;

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

} // namespace detail

//-------------------------------------------------------------------------------------------------

StreamSocket::StreamSocket(
    std::unique_ptr<AbstractStreamSocket> delegate,
    bool isServerSide)
    :
    base_type(delegate.get()),
    m_delegate(std::move(delegate)),
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

    m_socketToPipelineAdapter =
        std::make_unique<detail::StreamSocketToTwoWayPipelineAdapter>(
            m_delegate.get(), m_asyncTransformingChannel.get());

    bindToAioThread(m_delegate->getAioThread());
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

void StreamSocket::pleaseStopSync()
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
    m_handshakeTimer.bindToAioThread(aioThread);
}

bool StreamSocket::connect(
    const SocketAddress& endpoint,
    std::chrono::milliseconds timeout)
{
    if (!base_type::connect(endpoint, timeout))
        return false;

    switchToSyncModeIfNeeded();
    m_sslPipeline->setServerName(endpoint.address.toStdString());
    return m_sslPipeline->performHandshake();
}

void StreamSocket::connectAsync(
    const SocketAddress& endpoint,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    base_type::connectAsync(
        endpoint,
        [this, endpoint, handler = std::move(handler)](
            SystemError::ErrorCode connectResultCode) mutable
        {
            if (connectResultCode != SystemError::noError)
                return handler(connectResultCode);

            m_sslPipeline->setServerName(endpoint.address.toStdString());
            handshakeAsync(std::move(handler));
        });
}

int StreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    switchToSyncModeIfNeeded();
    // TODO: #ak setFlagsForCallsInThread is a hack. Have to pass flags
    // via m_sslPipeline->read call or get rid of flags in StreamSocket::recv at all.
    if (flags != 0)
        m_socketToPipelineAdapter->setFlagsForCallsInThread(std::this_thread::get_id(), flags);
    const int result = m_sslPipeline->read(buffer, bufferLen);
    if (flags != 0)
        m_socketToPipelineAdapter->setFlagsForCallsInThread(std::this_thread::get_id(), 0);
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

bool StreamSocket::isEncryptionEnabled() const
{
    return true;
}

void StreamSocket::handshakeAsync(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    switchToAsyncModeIfNeeded();

    m_handshakeHandler = std::move(handler);

    dispatch(
        [this]()
        {
            unsigned int sendTimeoutMs = 0;
            getSendTimeout(&sendTimeoutMs);
            std::chrono::milliseconds handshakeTimout(sendTimeoutMs);

            if (handshakeTimout != network::kNoTimeout)
                startHandshakeTimer(handshakeTimout);

            doHandshake();
        });
}

std::string StreamSocket::serverName() const
{
    return m_sslPipeline->serverNameFromClientHello();
}

void StreamSocket::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    // Performing handshake (part of connect) and cancellation of connect has been requested?
    if (eventType == aio::EventType::etWrite || eventType == aio::EventType::etNone)
    {
        m_handshakeTimer.cancelSync();
        if (!m_sslPipeline->isHandshakeCompleted())
        {
            // Then we cancel all I/O since handshake invokes both send & recv.
            eventType = aio::EventType::etNone;
            m_asyncTransformingChannel->cancelPostedCallsSync();
        }
    }

    m_delegate->cancelIOSync(eventType);
    m_asyncTransformingChannel->cancelIOSync(eventType);
}

void StreamSocket::startHandshakeTimer(std::chrono::milliseconds timout)
{
    m_handshakeTimer.start(
        timout,
        [this]()
        {
            cancelIoInAioThread(aio::EventType::etWrite);
            nx::utils::swapAndCall(m_handshakeHandler, SystemError::timedOut);
        });
}

void StreamSocket::doHandshake()
{
    m_asyncTransformingChannel->sendAsync(
        m_emptyBuffer,
        [this](
            SystemError::ErrorCode systemErrorCode,
            std::size_t /*bytesSent*/)
        {
            m_handshakeTimer.pleaseStopSync();
            nx::utils::swapAndCall(m_handshakeHandler, systemErrorCode);
        });
}

void StreamSocket::stopWhileInAioThread()
{
    m_asyncTransformingChannel->pleaseStopSync();
    m_delegate->pleaseStopSync();
    m_handshakeTimer.pleaseStopSync();
}

void StreamSocket::switchToSyncModeIfNeeded()
{
    m_sslPipeline->setInput(m_socketToPipelineAdapter.get());
    m_sslPipeline->setOutput(m_socketToPipelineAdapter.get());
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
