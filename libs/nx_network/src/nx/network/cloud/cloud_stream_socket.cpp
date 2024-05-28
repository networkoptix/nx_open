// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_stream_socket.h"

#include <future>

#include <nx/network/aio/aio_service.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>

#include "tunnel/outgoing_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"

namespace nx::network::cloud {

CloudStreamSocket::CloudStreamSocket(int ipVersion): m_ipVersion(ipVersion)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);

    // TODO: #akolesnikov User MUST be able to bind this object to any aio thread
    m_socketAttributes.aioThread = m_aioThreadBinder.getAioThread();

    bindToAioThread(m_aioThreadBinder.getAioThread());
}

CloudStreamSocket::~CloudStreamSocket()
{
    pleaseStopSync();

    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

aio::AbstractAioThread* CloudStreamSocket::getAioThread() const
{
    return m_aioThreadBinder.getAioThread();
}

void CloudStreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_aioThreadBinder.bindToAioThread(aioThread);
    if (m_socketDelegate)
        m_socketDelegate->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
    m_readIoBinder.bindToAioThread(aioThread);
    m_writeIoBinder.bindToAioThread(aioThread);
    if (m_connector)
        m_connector->bindToAioThread(aioThread);

    m_socketAttributes.aioThread = aioThread;
}

bool CloudStreamSocket::getProtocol(int* protocol) const
{
    return m_socketDelegate
        ? m_socketDelegate->getProtocol(protocol)
        : false;
}

bool CloudStreamSocket::bind(const SocketAddress& localAddress)
{
    // TODO: #akolesnikov just ignoring for now.
    // Usually, we do not care about the exact port on TCP client socket.
    static_cast<void>(localAddress);
    return true;
}

SocketAddress CloudStreamSocket::getLocalAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLocalAddress();

    return SocketAddress();
}

bool CloudStreamSocket::close()
{
    m_isClosed = true;

    if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
        promisePtr->set_value(SystemError::interrupted);

    if (m_socketDelegate)
        return m_socketDelegate->close();

    return true;
}

bool CloudStreamSocket::isClosed() const
{
    return m_isClosed.load();
}

bool CloudStreamSocket::shutdown()
{
    m_isClosed = true;

    if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
        promisePtr->set_value(SystemError::interrupted);

    if (m_socketDelegate)
        return m_socketDelegate->shutdown();

    return true;
}

AbstractSocket::SOCKET_HANDLE CloudStreamSocket::handle() const
{
    if (m_socketDelegate)
        return m_socketDelegate->handle();

    SystemError::setLastErrorCode(SystemError::notSupported);
    return -1;
}

nx::network::Pollable* CloudStreamSocket::pollable()
{
    return m_socketDelegate ? m_socketDelegate->pollable() : nullptr;
}

bool CloudStreamSocket::connect(const SocketAddress& addr, std::chrono::milliseconds timeout)
{
    // The function is blocking. So, it MUST NOT be called within an AIO thread.
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    std::promise<SystemError::ErrorCode> promise;
    {
        SocketResultPromisePtr expected = nullptr;
        if (!m_connectPromisePtr.compare_exchange_strong(expected, &promise))
        {
            NX_ASSERT(false, "Concurrent CloudStreamSocket::connect invocation. addr = %1", addr);
            SystemError::setLastErrorCode(SystemError::already);
            return false;
        }
    }

    // NOTE: This check MUST be after setting m_connectPromisePtr to avoid race condition with
    // CloudStreamSocket::shutdown() which also accesses both m_connectPromisePtr and m_isClosed.
    if (m_isClosed)
    {
        // NOTE: a concurrent thread may invoke shutdown or close setting the promise to signalled state.
        // So, making sure the promise is always set to the signalled state and waiting on it.
        if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
            promisePtr->set_value(SystemError::badDescriptor);
        SystemError::setLastErrorCode(promise.get_future().get());
        return false;
    }

    std::optional<std::tuple<
        SystemError::ErrorCode,
        std::optional<TunnelAttributes>,
        std::unique_ptr<AbstractStreamSocket>>> connectResult;

    // Keeping the connect state locally, within the current function call.
    detail::CloudStreamSocketConnector connector(m_ipVersion, addr);
    connector.connect(
        timeout,
        getSocketAttributes(),
        [this, &connectResult](SystemError::ErrorCode err, auto&&... args)
        {
            if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
            {
                connectResult.emplace(err, std::forward<decltype(args)>(args)...);
                promisePtr->set_value(err);
            }
        });

    // The connect has either completed or was cancelled by either shutdown() or close().
    const auto resultCode = promise.get_future().get();

    // Stopping ongoing connect if it was cancelled.
    connector.pleaseStopSync();

    if (resultCode != SystemError::noError)
    {
        SystemError::setLastErrorCode(resultCode);
        return false;
    }

    if (connectResult)
    {
        std::apply(
            [this](auto&&... args) { saveConnectResult(std::forward<decltype(args)>(args)...); },
            std::move(*connectResult));
    }
    else
    {
        NX_ASSERT(false, "Zero result code and missing result attributes is a bug. addr = %1", addr);
        SystemError::setLastErrorCode(SystemError::invalidData);
        return false;
    }

    return true;
}

int CloudStreamSocket::recv(void* buffer, std::size_t bufferLen, int flags)
{
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    if (!m_socketDelegate)
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return -1;
    }

    return m_socketDelegate->recv(buffer, bufferLen, flags);
}

int CloudStreamSocket::send(const void* buffer, std::size_t bufferLen)
{
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    if (!m_socketDelegate)
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return -1;
    }

    return m_socketDelegate->send(buffer, bufferLen);
}

SocketAddress CloudStreamSocket::getForeignAddress() const
{
    if (m_socketDelegate)
    {
        if (m_cloudTunnelAttributes.addressType == AddressType::cloud)
            return m_cloudTunnelAttributes.remotePeerName;

        return m_socketDelegate->getForeignAddress();
    }

    SystemError::setLastErrorCode(SystemError::notConnected);
    return SocketAddress();
}

bool CloudStreamSocket::isConnected() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isConnected();

    return false;
}

void CloudStreamSocket::cancelIoInAioThread(aio::EventType eventType)
{
    if (eventType == aio::etNone || eventType == aio::etTimedOut)
        m_timer.cancelSync();

    if (eventType == aio::etNone || eventType == aio::etRead)
        m_readIoBinder.pleaseStopSync();

    if (eventType == aio::etNone || eventType == aio::etWrite)
    {
        m_writeIoBinder.pleaseStopSync();
        m_connector.reset(); //< Cancelling connect.
    }

    if (m_socketDelegate)
        m_socketDelegate->cancelIOSync(eventType);
}

void CloudStreamSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder.post(std::move(handler));
}

void CloudStreamSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder.dispatch(std::move(handler));
}

void CloudStreamSocket::connectAsync(
    const SocketAddress& address,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    NX_VERBOSE(this, "connectAsync. %1", address);

    unsigned int sendTimeoutMillis = 0;
    if (!getSendTimeout(&sendTimeoutMillis))
        return handler(SystemError::getLastOSErrorCode());

    m_connector = std::make_unique<detail::CloudStreamSocketConnector>(m_ipVersion, address);
    m_connector->bindToAioThread(getAioThread());
    m_connector->connect(
        std::chrono::milliseconds(sendTimeoutMillis),
        getSocketAttributes(),
        [this, handler = std::move(handler)](
            SystemError::ErrorCode err, auto&&... args)
        {
            saveConnectResult(err, std::forward<decltype(args)>(args)...);
            handler(err);
        });
}

void CloudStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    IoCompletionHandler handler)
{
    if (m_socketDelegate)
    {
        m_socketDelegate->readSomeAsync(buf, std::move(handler));
    }
    else
    {
        m_readIoBinder.post(
            [handler = std::move(handler)]()
            {
                handler(SystemError::notConnected, 0);
            });
    }
}

void CloudStreamSocket::sendAsync(
    const nx::Buffer* buf,
    IoCompletionHandler handler)
{
    if (m_socketDelegate)
    {
        m_socketDelegate->sendAsync(buf, std::move(handler));
    }
    else
    {
        m_writeIoBinder.post(
            [handler = std::move(handler)]()
            {
                handler(SystemError::notConnected, 0);
            });
    }
}

void CloudStreamSocket::registerTimer(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer.start(timeout, std::move(handler));
}

void CloudStreamSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder.pleaseStop(
        [this, handler = std::move(handler)]()
        {
            stopWhileInAioThread();
            handler();
        });
}

void CloudStreamSocket::pleaseStopSync()
{
    m_aioThreadBinder.executeInAioThreadSync([this]() { stopWhileInAioThread(); });
}

bool CloudStreamSocket::isInSelfAioThread() const
{
    return m_aioThreadBinder.isInSelfAioThread();
}

std::string CloudStreamSocket::getForeignHostName() const
{
    if (!m_cloudTunnelAttributes.remotePeerName.empty())
        return m_cloudTunnelAttributes.remotePeerName;
    if (m_socketDelegate)
        return m_socketDelegate->getForeignHostName();
    return std::string();
}

SystemError::ErrorCode CloudStreamSocket::applyRealNonBlockingMode(
    AbstractStreamSocket* streamSocket)
{
    SystemError::ErrorCode errorCode = SystemError::noError;

    if (!m_socketAttributes.nonBlockingMode)
    {
        // Restoring default non-blocking mode on socket.
        if (!streamSocket->setNonBlockingMode(false))
        {
            errorCode = SystemError::getLastOSErrorCode();
            NX_ASSERT(errorCode != SystemError::noError);
        }
    }

    return errorCode;
}

void CloudStreamSocket::saveConnectResult(
    SystemError::ErrorCode errorCode,
    std::optional<TunnelAttributes> cloudTunnelAttributes,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    NX_VERBOSE(this, "Connect completed with result %1", errorCode);

    if (errorCode == SystemError::noError)
    {
        errorCode = applyRealNonBlockingMode(connection.get());
        if (errorCode != SystemError::noError)
            connection.reset();
        if (cloudTunnelAttributes)
        {
            NX_VERBOSE(this, "Got connection to [%1]", cloudTunnelAttributes->remotePeerName);
            m_cloudTunnelAttributes = std::move(*cloudTunnelAttributes);
        }
        else
        {
            NX_VERBOSE(this, "Got connection without tunnel attributes");
        }
    }

    if (errorCode == SystemError::noError)
    {
        NX_ASSERT(connection->getAioThread() == m_aioThreadBinder.getAioThread());
        m_socketDelegate = std::move(connection);
        setDelegate(m_socketDelegate.get());
    }
    else
    {
        NX_ASSERT(!connection);
    }
}

void CloudStreamSocket::stopWhileInAioThread()
{
    m_timer.pleaseStopSync();
    m_readIoBinder.pleaseStopSync();
    m_writeIoBinder.pleaseStopSync();
    if (m_socketDelegate)
    {
        m_socketDelegate->pleaseStopSync();
        setDelegate(nullptr);
    }
    m_connector.reset();

    m_aioThreadBinder.pleaseStopSync();
}

} // namespace nx::network::cloud
