#include "abstract_socket.h"

#include <nx/utils/thread/mutex_lock_analyzer.h>
#include <nx/utils/unused.h>

#include "aio/pollable.h"

//-------------------------------------------------------------------------------------------------
// class AbstractSocket.

bool AbstractSocket::bind(const QString& localAddress, unsigned short localPort)
{
    return bind(SocketAddress(localAddress, localPort));
}

bool AbstractSocket::isInSelfAioThread() const
{
    // AbstractSocket does not provide const pollable() just to simplify implementation, so it is
    // safe to perform a const_cast.
    const nx::network::Pollable* p = const_cast<AbstractSocket*>(this)->pollable();
    return p->isInSelfAioThread();
}

//-------------------------------------------------------------------------------------------------
// class AbstractCommunicatingSocket.

bool AbstractCommunicatingSocket::connect(
    const QString& foreignAddress,
    unsigned short foreignPort,
    unsigned int timeoutMillis)
{
    // TODO: #ak this method MUST replace the previous one
    return connect(SocketAddress(foreignAddress, foreignPort), timeoutMillis);
}

bool AbstractCommunicatingSocket::connect(
    const SocketAddress& remoteSocketAddress,
    std::chrono::milliseconds timeoutMillis)
{
    // TODO: #ak this method MUST replace the previous one
    return connect(remoteSocketAddress, timeoutMillis.count());
}

int AbstractCommunicatingSocket::send(const QByteArray& data)
{
    return send(data.constData(), data.size());
}

QString AbstractCommunicatingSocket::getForeignHostName() const
{
    return getForeignAddress().address.toString();
}

void AbstractCommunicatingSocket::registerTimer(
    unsigned int timeout,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    return registerTimer(
        std::chrono::milliseconds(timeout),
        std::move(handler));
}

void AbstractCommunicatingSocket::readAsyncAtLeast(
    nx::Buffer* const buffer, size_t minimalSize,
    IoCompletionHandler handler)
{
    NX_CRITICAL(
        buffer->capacity() >= buffer->size() + static_cast<int>(minimalSize),
        "Not enough space in the buffer!");

    readAsyncAtLeastImpl(
        buffer, minimalSize, std::move(handler),
        static_cast<size_t>(buffer->size()));
}

void AbstractCommunicatingSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    cancelIOAsync(nx::network::aio::EventType::etNone, std::move(handler));
}

void AbstractCommunicatingSocket::pleaseStopSync(bool checkForLocks)
{
    #ifdef USE_OWN_MUTEX
        if (checkForLocks)
        {
            const auto pollablePtr = pollable();
            if (!pollablePtr || !pollablePtr->isInSelfAioThread())
                MutexLockAnalyzer::instance()->expectNoLocks();
        }
    #else
        QN_UNUSED(checkForLocks);
    #endif

    cancelIOSync(nx::network::aio::EventType::etNone);
}

QString AbstractCommunicatingSocket::idForToStringFromPtr() const
{
    return lm("%1->%2").args(getLocalAddress(), getForeignAddress());
}

void AbstractCommunicatingSocket::readAsyncAtLeastImpl(
    nx::Buffer* const buffer, size_t minimalSize,
    IoCompletionHandler handler,
    size_t initBufSize)
{
    readSomeAsync(
        buffer,
        [this, buffer, minimalSize, handler = std::move(handler), initBufSize](
            SystemError::ErrorCode code, size_t size) mutable
        {
            if (code != SystemError::noError || size == 0 ||
                static_cast<size_t>(buffer->size()) >= initBufSize + minimalSize)
            {
                return handler(
                    code, static_cast<size_t>(buffer->size()) - initBufSize);
            }

            readAsyncAtLeastImpl(
                buffer, minimalSize, std::move(handler), initBufSize);
        });
}

//-------------------------------------------------------------------------------------------------

QString AbstractStreamServerSocket::idForToStringFromPtr() const
{
    return lm("%1").args(getLocalAddress());
}

bool AbstractDatagramSocket::setDestAddr(
    const QString& foreignAddress,
    unsigned short foreignPort)
{
    return setDestAddr(SocketAddress(foreignAddress, foreignPort));
}

bool AbstractDatagramSocket::sendTo(
    const void* buffer,
    unsigned int bufferLen,
    const QString& foreignAddress,
    unsigned short foreignPort)
{
    return sendTo(
        buffer,
        bufferLen,
        SocketAddress(foreignAddress, foreignPort));
}

bool AbstractDatagramSocket::sendTo(
    const nx::Buffer& buf,
    const SocketAddress& foreignAddress)
{
    return sendTo(buf.constData(), buf.size(), foreignAddress);
}
