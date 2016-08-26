
#include "abstract_socket.h"


////////////////////////////////////////////////////////////
//// class AbstractSocket
////////////////////////////////////////////////////////////
bool AbstractSocket::bind(const QString& localAddress, unsigned short localPort)
{
    return bind(SocketAddress(localAddress, localPort));
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

void AbstractCommunicatingSocket::pleaseStopSync()
{
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

KeepAliveOptions::KeepAliveOptions(int timeSec, int intervalSec, int probeCount):
    timeSec(timeSec),
    intervalSec(intervalSec),
    probeCount(probeCount)
{
}

bool KeepAliveOptions::operator==(const KeepAliveOptions& rhs) const
{
    return timeSec == rhs.timeSec
        && intervalSec == rhs.intervalSec
        && probeCount == rhs.probeCount;
}

QString KeepAliveOptions::toString() const
{
    // TODO: Use JSON serrialization instead?
    return lm("{ %1, %2, %3 }").arg(timeSec).arg(intervalSec).arg(probeCount);
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
    options.timeSec = split[0].trimmed().toInt();
    options.intervalSec = split[1].trimmed().toInt();
    options.probeCount = split[2].trimmed().toInt();
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
