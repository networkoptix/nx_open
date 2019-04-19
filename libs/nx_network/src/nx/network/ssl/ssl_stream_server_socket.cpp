#include "ssl_stream_server_socket.h"

#include "encryption_detecting_stream_socket.h"
#include "ssl_stream_socket.h"

namespace nx {
namespace network {
namespace ssl {

namespace detail {

template<typename Delegate>
class AcceptedSslStreamSocketWrapper:
    public AbstractAcceptedSslStreamSocketWrapper
{
    using base_type = AbstractAcceptedSslStreamSocketWrapper;

public:
    AcceptedSslStreamSocketWrapper(
        std::unique_ptr<Delegate> delegate)
        :
        base_type(delegate.get()),
        m_delegate(std::move(delegate))
    {
    }

    virtual bool isEncryptionEnabled() const override
    {
        return m_delegate->isEncryptionEnabled();
    }

    virtual void handshakeAsync(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_delegate->handshakeAsync(std::move(handler));
    }

    virtual std::string serverName() const override
    {
        return m_delegate->serverName();
    }

private:
    std::unique_ptr<Delegate> m_delegate;
};

template<typename SslSocket>
std::unique_ptr<AcceptedSslStreamSocketWrapper<SslSocket>> makeAcceptedSslStreamSocketWrapper(
    std::unique_ptr<AbstractStreamSocket> delegate)
{
    return std::make_unique<AcceptedSslStreamSocketWrapper<SslSocket>>(
        std::make_unique<SslSocket>(std::move(delegate)));
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

StreamServerSocket::StreamServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> delegate,
    EncryptionUse encryptionUse)
    :
    base_type(delegate.get()),
    m_acceptor(
        std::move(delegate),
        [this](auto&&... args) { return createSocketWrapper(std::move(args)...); }),
    m_encryptionUse(encryptionUse)
{
    bindToAioThread(m_acceptor.getAioThread());
}

StreamServerSocket::~StreamServerSocket()
{
    m_acceptor.pleaseStopSync();
}

void StreamServerSocket::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void StreamServerSocket::pleaseStopSync()
{
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

void StreamServerSocket::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_acceptor.bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

bool StreamServerSocket::setNonBlockingMode(bool value)
{
    m_nonBlockingModeEnabled = value;
    return true;
}

bool StreamServerSocket::getNonBlockingMode(bool* value) const
{
    *value = m_nonBlockingModeEnabled;
    return true;
}

bool StreamServerSocket::listen(int backlog)
{
    // Always switching underlying socket to non-blocking mode as required by m_acceptor.
    if (!base_type::setNonBlockingMode(true) ||
        !base_type::listen(backlog))
    {
        return false;
    }

    m_acceptor.setReadyConnectionQueueSize(backlog);
    m_acceptor.start();
    return true;
}

void StreamServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    if (!m_nonBlockingModeEnabled)
    {
        return post(
            [handler = std::move(handler)]()
            {
                handler(SystemError::notSupported, nullptr);
            });
    }

    acceptAsyncInternal(std::move(handler));
}

std::unique_ptr<AbstractStreamSocket> StreamServerSocket::accept()
{
    auto connection = m_nonBlockingModeEnabled
        ? acceptNonBlocking()
        : acceptBlocking();

    if (!connection || !connection->setNonBlockingMode(false))
        return nullptr;

    return connection;
}

void StreamServerSocket::cancelIoInAioThread()
{
    base_type::cancelIoInAioThread();

    m_acceptor.cancelIOSync();
    m_timer.cancelSync();
}

std::unique_ptr<detail::AbstractAcceptedSslStreamSocketWrapper>
    StreamServerSocket::createSocketWrapper(
        std::unique_ptr<AbstractStreamSocket> delegate)
{
    switch (m_encryptionUse)
    {
        case EncryptionUse::always:
            return detail::makeAcceptedSslStreamSocketWrapper<ServerSideStreamSocket>(
                std::move(delegate));

        case EncryptionUse::autoDetectByReceivedData:
            return detail::makeAcceptedSslStreamSocketWrapper<EncryptionDetectingStreamSocket>(
                std::move(delegate));

        default:
            NX_ASSERT(false);
            return nullptr;
    }
}

std::unique_ptr<AbstractStreamSocket> StreamServerSocket::acceptNonBlocking()
{
    auto connection = m_acceptor.getNextConnectionIfAny();
    if (!connection)
        SystemError::setLastErrorCode(SystemError::wouldBlock);

    return connection;
}

std::unique_ptr<AbstractStreamSocket> StreamServerSocket::acceptBlocking()
{
    std::promise<std::tuple<
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>>
        > accepted;

    acceptAsyncInternal(
        [&accepted](
            SystemError::ErrorCode systemErrorCode,
            std::unique_ptr<AbstractStreamSocket> connection)
        {
            accepted.set_value(std::make_tuple(systemErrorCode, std::move(connection)));
        });

    auto result = accepted.get_future().get();
    auto connection = std::move(std::get<1>(result));
    if (!connection)
    {
        SystemError::setLastErrorCode(std::get<0>(result));
        return nullptr;
    }

    if (!connection->setNonBlockingMode(false))
        return nullptr;

    return connection;
}

void StreamServerSocket::acceptAsyncInternal(AcceptCompletionHandler handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            unsigned int timeout = 0;
            if (!getRecvTimeout(&timeout))
            {
                return post(
                    [handler = std::move(handler),
                        errorCode = SystemError::getLastOSErrorCode()]()
                    {
                        handler(errorCode, nullptr);
                    });
            }

            m_userHandler = std::move(handler);

            if (timeout > 0)
                startTimer(std::chrono::milliseconds(timeout));
            else
                m_timer.cancelSync();

            m_acceptor.acceptAsync(
                [this](auto&&... args) { onAccepted(std::move(args)...); });
        });
}

void StreamServerSocket::startTimer(std::chrono::milliseconds timeout)
{
    m_timer.start(
        timeout,
        [this]()
        {
            m_acceptor.cancelIOSync();
            nx::utils::swapAndCall(m_userHandler, SystemError::timedOut, nullptr);
        });
}

void StreamServerSocket::onAccepted(
    SystemError::ErrorCode systemErrorCode,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    m_timer.cancelSync();
    nx::utils::swapAndCall(m_userHandler, systemErrorCode, std::move(connection));
}

void StreamServerSocket::stopWhileInAioThread()
{
    m_acceptor.pleaseStopSync();
    m_timer.pleaseStopSync();
}

} // namespace ssl
} // namespace network
} // namespace nx
