#include "abstract_socket.h"

#include <nx/network/aio/pollable.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

////////////////////////////////////////////////////////////
//// class AbstractSocket
////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////
//// class AbstractCommunicatingSocket
////////////////////////////////////////////////////////////

bool AbstractCommunicatingSocket::connect(
    const QString& foreignAddress,
    unsigned short foreignPort,
    unsigned int timeoutMillis)
{
    //TODO #ak this method MUST replace the previous one
    return connect(SocketAddress(foreignAddress, foreignPort), timeoutMillis);
}

bool AbstractCommunicatingSocket::connect(
    const SocketAddress& remoteSocketAddress,
    std::chrono::milliseconds timeoutMillis)
{
    //TODO #ak this method MUST replace the previous one
    return connect(remoteSocketAddress, timeoutMillis.count());
}

int AbstractCommunicatingSocket::send(const QByteArray& data)
{
    return send(data.constData(), data.size());
}

int AbstractCommunicatingSocket::send(const QnByteArray& data)
{
    return send(data.constData(), data.size());
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
    std::function<void(SystemError::ErrorCode, size_t)> handler)
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
    #endif

    cancelIOSync(nx::network::aio::EventType::etNone);
}

void AbstractCommunicatingSocket::readAsyncAtLeastImpl(
    nx::Buffer* const buffer, size_t minimalSize,
    std::function<void(SystemError::ErrorCode, size_t)> handler,
    size_t initBufSize)
{
    readSomeAsync(
        buffer,
        [this, buffer, minimalSize, handler = std::move(handler), initBufSize](
            SystemError::ErrorCode code, size_t size)
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

std::chrono::seconds KeepAliveOptions::maxDelay() const
{
    return time + interval * probeCount;
}

KeepAliveOptions::KeepAliveOptions(
    std::chrono::seconds time, std::chrono::seconds interval, size_t probeCount)
:
    time(time),
    interval(interval),
    probeCount(probeCount)
{
}

KeepAliveOptions::KeepAliveOptions(size_t time, size_t interval, size_t count):
    KeepAliveOptions(std::chrono::seconds(time), std::chrono::seconds(interval), count)
{
}

bool KeepAliveOptions::operator==(const KeepAliveOptions& rhs) const
{
    return time == rhs.time && interval == rhs.interval && probeCount == rhs.probeCount;
}

QString KeepAliveOptions::toString() const
{
    // TODO: Use JSON serrialization instead?
    return lm("{ %1, %2, %3 }").arg(time.count()).arg(interval.count()).arg(probeCount);
}

boost::optional<KeepAliveOptions> KeepAliveOptions::fromString(const QString& string)
{
    QStringRef stringRef(&string);
    if (stringRef.startsWith(QLatin1String("{")) && stringRef.endsWith(QLatin1String("}")))
        stringRef = stringRef.mid(1, stringRef.length() - 2);

    const auto split = stringRef.split(QLatin1String(","));
    if (split.size() != 3)
        return boost::none;

    KeepAliveOptions options;
    options.time = std::chrono::seconds((size_t) split[0].trimmed().toUInt());
    options.interval = std::chrono::seconds((size_t) split[1].trimmed().toUInt());
    options.probeCount = (size_t) split[2].trimmed().toUInt();
    return options;
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
