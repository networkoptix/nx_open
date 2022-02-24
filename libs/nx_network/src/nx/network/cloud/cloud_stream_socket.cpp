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

CloudStreamSocket::CloudStreamSocket(int ipVersion):
    m_connectPromisePtr(nullptr),
    m_ipVersion(ipVersion),
    m_stoppedFuture(m_stoppedPromise.get_future())
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
    if (m_multipleAddressConnector)
        m_multipleAddressConnector->bindToAioThread(aioThread);

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
    // Usually, we do not care about the exact port on tcp client socket.
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
    m_isTerminated = true;

    if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
        promisePtr->set_value(std::make_pair(SystemError::interrupted, 0));

    if (m_socketDelegate)
        return m_socketDelegate->close();

    return true;
}

bool CloudStreamSocket::isClosed() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isClosed();

    return true;
}

bool CloudStreamSocket::shutdown()
{
    m_isTerminated = true;

    if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
        promisePtr->set_value(std::make_pair(SystemError::interrupted, 0));

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

bool CloudStreamSocket::connect(
    const SocketAddress& remoteAddress,
    std::chrono::milliseconds timeout)
{
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    unsigned int sendTimeoutBak = 0;
    if (!getSendTimeout(&sendTimeoutBak))
        return false;
    if (timeout != nx::network::kNoTimeout)
    {
        if (!setSendTimeout(timeout.count()))
            return false;
    }

    nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    {
        if (m_isTerminated)
        {
            SystemError::setLastErrorCode(SystemError::interrupted);
            return false;
        }

        SocketResultPrimisePtr expected = nullptr;
        if (!m_connectPromisePtr.compare_exchange_strong(expected, &promise))
        {
            NX_ASSERT(false);
            SystemError::setLastErrorCode(SystemError::already);
            return false;
        }
    }

    connectAsync(
        remoteAddress,
        [this](SystemError::ErrorCode code)
        {
            // We use post to ensure that socket is not used by aio sub-system anymore.
            m_writeIoBinder.post(
                [this, code]()
                {
                    if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
                        promisePtr->set_value(std::make_pair(code, 0));
                });
        });

    auto result = promise.get_future().get().first;
    if (result != SystemError::noError)
    {
        SystemError::setLastErrorCode(result);
        return false;
    }

    return setSendTimeout(sendTimeoutBak);
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
    if (eventType == aio::etWrite || eventType == aio::etNone)
    {
        m_asyncConnectGuard->terminate(); //< Breaks outgoing connects.
        nx::network::SocketGlobals::addressResolver().cancel(this);
    }

    if (eventType == aio::etNone || eventType == aio::etTimedOut)
        m_timer.cancelSync();

    if (eventType == aio::etNone || eventType == aio::etRead)
        m_readIoBinder.pleaseStopSync();

    if (eventType == aio::etNone || eventType == aio::etWrite)
    {
        m_writeIoBinder.pleaseStopSync();
        m_multipleAddressConnector.reset(); //< Cancelling connect.
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
    NX_VERBOSE(this, nx::format("connectAsync. %1").arg(address));

    nx::network::SocketGlobals::addressResolver().resolveAsync(
        address.address,
        [this, operationGuard = m_asyncConnectGuard.sharedGuard(),
            port = address.port, handler = std::move(handler)](
                SystemError::ErrorCode code, std::deque<AddressEntry> dnsEntries) mutable
        {
            NX_VERBOSE(this, nx::format("Address resolved. Code: %1, entries: %2")
                .args(code, dnsEntries.size()));

            if (auto lock = operationGuard->lock())
            {
                m_writeIoBinder.post(
                    [this, port, handler = std::move(handler),
                        code, dnsEntries = std::move(dnsEntries)]() mutable
                    {
                        if (code != SystemError::noError)
                            return handler(code);

                        if (dnsEntries.empty())
                        {
                            NX_ASSERT(false);
                            return handler(SystemError::hostNotFound);
                        }

                        connectToEntriesAsync(std::move(dnsEntries), port, std::move(handler));
                    });
            }
        },
        NatTraversalSupport::enabled,
        m_ipVersion,
        this);
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
    if (m_isStopped.exchange(true))
    {
        handler();
        return;
    }

    m_aioThreadBinder.pleaseStop(
        [this, handler = std::move(handler)]()
        {
            stopWhileInAioThread();
            handler();
        });
}

void CloudStreamSocket::pleaseStopSync()
{
    if (m_isStopped.exchange(true))
    {
        m_stoppedFuture.wait();
        return;
    }

    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
    }
    else
    {
        m_aioThreadBinder.pleaseStop([this]() { stopWhileInAioThread(); });
        m_stoppedFuture.wait();
    }
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

void CloudStreamSocket::connectToEntriesAsync(
    std::deque<AddressEntry> dnsEntries,
    int port,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    using namespace std::placeholders;

    unsigned int sendTimeoutMillis = 0;
    if (!getSendTimeout(&sendTimeoutMillis))
        return handler(SystemError::getLastOSErrorCode());

    for (auto& entry: dnsEntries)
    {
        if (entry.type != AddressType::direct)
            continue;

        auto portAttrIter = std::find_if(
            entry.attributes.begin(), entry.attributes.end(),
            [](const AddressAttribute& attr) { return attr.type == AddressAttributeType::port; });
        if (portAttrIter == entry.attributes.end())
        {
            entry.attributes.push_back(
                AddressAttribute(AddressAttributeType::port, port));
        }
    }

    m_connectHandler = std::move(handler);

    m_multipleAddressConnector = std::make_unique<AnyAccessibleAddressConnector>(
        m_ipVersion,
        std::move(dnsEntries));
    m_multipleAddressConnector->bindToAioThread(getAioThread());
    m_multipleAddressConnector->connectAsync(
        std::chrono::milliseconds(sendTimeoutMillis),
        getSocketAttributes(),
        std::bind(&CloudStreamSocket::onConnectDone, this, _1, _2, _3));
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

void CloudStreamSocket::onConnectDone(
    SystemError::ErrorCode errorCode,
    std::optional<TunnelAttributes> cloudTunnelAttributes,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    NX_VERBOSE(this, nx::format("Connect completed with result %1").arg(errorCode));

    if (errorCode == SystemError::noError)
    {
        errorCode = applyRealNonBlockingMode(connection.get());
        if (errorCode != SystemError::noError)
            connection.reset();
        if (cloudTunnelAttributes)
        {
            NX_VERBOSE(this, nx::format("Got connection to [%1]").arg(cloudTunnelAttributes->remotePeerName));
            m_cloudTunnelAttributes = std::move(*cloudTunnelAttributes);
        }
        else
        {
            NX_VERBOSE(this, nx::format("Got connection without tunnel attributes"));
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

    nx::utils::swapAndCall(m_connectHandler, errorCode);
}

void CloudStreamSocket::stopWhileInAioThread()
{
    m_asyncConnectGuard->terminate(); //< Breaks outgoing connects.
    nx::network::SocketGlobals::addressResolver().cancel(this);

    m_timer.pleaseStopSync();
    m_readIoBinder.pleaseStopSync();
    m_writeIoBinder.pleaseStopSync();
    if (m_socketDelegate)
    {
        m_socketDelegate->pleaseStopSync();
        setDelegate(nullptr);
    }
    m_multipleAddressConnector.reset();

    m_aioThreadBinder.pleaseStopSync();

    m_stoppedPromise.set_value();
}

} // namespace nx::network::cloud
