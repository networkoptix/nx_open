// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ssl_stream_socket.h"

#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

#include "../aio/async_channel_adapter.h"
#include "../nx_network_ini.h"
#include "helpers.h"

namespace nx::network::ssl {

const std::function<
    bool(CertificateChainView /*chain*/, StreamSocket* /*socket*/)> kDefaultCertificateCheckCallback =
        [](CertificateChainView chain, StreamSocket* socket)
        {
            if (!ini().useDefaultSslCertificateVerificationByOs)
                return true;

            std::string message;
            const auto serverName = socket->serverName();
            if (verifyBySystemCertificates(chain, serverName, &message))
                return true;

            NX_VERBOSE(socket,
                "Default certificate verification for server `%1` is failed: %2",
                serverName, message);
            return false;
        };

const std::function<
    bool(CertificateChainView /*chain*/, StreamSocket* /*socket*/)> kAcceptAnyCertificateCallback;

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
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    Context* context,
    std::unique_ptr<AbstractStreamSocket> delegate,
    bool isServerSide,
    VerifyCertificateChainCallbackSync verifyChainCallback)
    :
    base_type(delegate.get()),
    m_delegate(std::move(delegate)),
    m_proxyConverter(nullptr)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++nx::network::SocketGlobals::instance().debugCounters().sslSocketCount;

    if (isServerSide)
    {
        m_sslPipeline = std::make_unique<AcceptingPipeline>(context);
        setSyncVerifyCertificateChainCallback(kAcceptAnyCertificateCallback);
    }
    else
    {
        m_sslPipeline = std::make_unique<ConnectingPipeline>(context);
        setSyncVerifyCertificateChainCallback(std::move(verifyChainCallback));
    }

    m_proxyConverter.setDelegate(m_sslPipeline.get());
    m_asyncTransformingChannel = std::make_unique<aio::StreamTransformingAsyncChannel>(
        aio::makeAsyncChannelAdapter(m_delegate.get()),
        &m_proxyConverter);

    NX_VERBOSE(this, "%1-side. Local %2, remote %3, socket %4, transforming channel %5",
        isServerSide ? "Server" : "Client",
        m_delegate->getLocalAddress(), m_delegate->getForeignAddress(),
        m_delegate.get(), m_asyncTransformingChannel.get());

    m_socketToPipelineAdapter =
        std::make_unique<detail::StreamSocketToTwoWayPipelineAdapter>(
            m_delegate.get(), m_asyncTransformingChannel.get());

    bindToAioThread(m_delegate->getAioThread());
}

StreamSocket::~StreamSocket()
{
    --nx::network::SocketGlobals::instance().debugCounters().sslSocketCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
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
    if (m_serverName)
        m_sslPipeline->setServerName(*m_serverName);
    else
        m_sslPipeline->setServerName(endpoint.address.toString());

    if (timeout != kNoTimeout)
    {
        // NOTE: SSL handshake includes recv and send calls. So, setting the timeout for both.
        if (!saveTimeouts())
            return false;

        if (!setRecvTimeout(timeout.count()) || !setSendTimeout(timeout.count()))
            return false;
    }

    const auto result = m_sslPipeline->performHandshake();
    std::optional<SystemError::ErrorCode> errorCodeBak;
    if (!result)
        errorCodeBak = SystemError::getLastOSErrorCode();

    if (timeout != kNoTimeout)
    {
        if (!restoreTimeouts())
            return false;
    }

    if (errorCodeBak)
        SystemError::setLastErrorCode(*errorCodeBak);

    return result;
}

bool StreamSocket::saveTimeouts()
{
    m_recvTimeoutBak = 0;
    m_sendTimeoutBak = 0;

    return getRecvTimeout(&*m_recvTimeoutBak) && getSendTimeout(&*m_sendTimeoutBak);
}

bool StreamSocket::restoreTimeouts()
{
    return setRecvTimeout(*m_recvTimeoutBak) && setSendTimeout(*m_sendTimeoutBak);
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

            if (m_serverName)
                m_sslPipeline->setServerName(*m_serverName);
            else
                m_sslPipeline->setServerName(endpoint.address.toString());
            handshakeAsync(std::move(handler));
        });
}

int StreamSocket::recv(void* buffer, std::size_t bufferLen, int flags)
{
    switchToSyncModeIfNeeded();
    // TODO: Remove usage of setFlagsForCallsInThread(). `flags` should be passed to
    // m_sslPipeline->read() or be removed from StreamSocket::recv().
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

int StreamSocket::send(const void* buffer, std::size_t bufferLen)
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
    const nx::Buffer* buffer,
    IoCompletionHandler handler)
{
    switchToAsyncModeIfNeeded();
    m_asyncTransformingChannel->sendAsync(
        buffer,
        [this, handler = std::move(handler)](
            SystemError::ErrorCode resultCode, std::size_t bytesTransferred)
        {
            if (resultCode != SystemError::noError)
                m_sslPipeline->setFailed(true);

            handler(resultCode, bytesTransferred);
        });
}

bool StreamSocket::isConnected() const
{
    return base_type::isConnected() && !m_sslPipeline->failed() && !m_sslPipeline->eof();
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

template<typename T>
void StreamSocket::setVerifyCertificateChainCallback(
    T&& func,
    bool (StreamSocket::*method)(CertificateChainView))
{
    if (!func)
    {
        if (m_sslPipeline)
            m_sslPipeline->setVerifyCertificateChainCallback(nullptr);
    }
    else
    {
        m_verifyCertificateChainCallback = std::move(func);
        if (m_sslPipeline)
            m_sslPipeline->setVerifyCertificateChainCallback(
                [this, method](auto&&... args)
                {
                    return (this->*method)(std::forward<decltype(args)>(args)...);
                });
    }
}

bool StreamSocket::syncVerifyCertificateChainCallbackHandler(
    CertificateChainView chain)
{
    return std::get<VerifyCertificateChainCallbackSync>(m_verifyCertificateChainCallback)(
        std::move(chain),
        this);
}

void StreamSocket::setSyncVerifyCertificateChainCallback(VerifyCertificateChainCallbackSync func)
{
    setVerifyCertificateChainCallback(
        func,
        &StreamSocket::syncVerifyCertificateChainCallbackHandler);
}

bool StreamSocket::asyncVerifyCertificateChainCallbackHandler(
    CertificateChainView chain)
{
    m_sslPipeline->pauseDataProcessingAfterHandshake(
        [this]() { m_asyncTransformingChannel->pause(); });
    const auto sharedGuard = m_asyncOperationGuard.sharedGuard();
    std::get<VerifyCertificateChainCallbackAsync>(m_verifyCertificateChainCallback)(
        std::move(chain),
        this,
        [this, sharedGuard](
            bool isOk)
        {
            if (auto lock = sharedGuard->lock())
                m_asyncTransformingChannel->dispatch(
                    [this, isOk]()
                    {
                        if (!isOk)
                        {
                            m_sslPipeline->shutdown();
                            m_sslPipeline->setFailed(true);
                        }
                        m_sslPipeline->resumeDataProcessing();
                        m_asyncTransformingChannel->resume();
                    });
        });

    // TLS session will be established, but user data transmission will be blocked until the end
    // of the verification process.
    return true;
}

void StreamSocket::setAsyncVerifyCertificateChainCallback(
    VerifyCertificateChainCallbackAsync func)
{
    setVerifyCertificateChainCallback(
        func,
        &StreamSocket::asyncVerifyCertificateChainCallbackHandler);
}

void StreamSocket::setServerName(const std::string& serverName)
{
    m_serverName = serverName;
    NX_VERBOSE(this, "%1(%2)", __func__, serverName);
    if (m_sslPipeline)
        m_sslPipeline->setServerName(serverName);
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
        &m_emptyBuffer,
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
        SystemError::getLastOSErrorCode() != SystemError::timedOut) //< TODO: #akolesnikov Come up with a better way to pass os error.
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
    Context* context,
    std::unique_ptr<AbstractStreamSocket> delegate,
    VerifyCertificateChainCallbackSync verifyChainCallback)
    :
    base_type(
        context, std::move(delegate), /*isServerSide*/ false, std::move(verifyChainCallback))
{
}

ServerSideStreamSocket::ServerSideStreamSocket(
    Context* context,
    std::unique_ptr<AbstractStreamSocket> delegate)
    :
    base_type(context, std::move(delegate), /*isServerSide*/ true, kAcceptAnyCertificateCallback)
{
}

} // namespace nx::network::ssl
