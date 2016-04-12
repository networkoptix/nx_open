
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

void AbstractCommunicatingSocket::readFixedAsync(
    nx::Buffer* const buf, size_t minimalSize,
    std::function<void(SystemError::ErrorCode, size_t)> handler,
    size_t baseSize)
{
    NX_CRITICAL(buf->capacity() >= minimalSize);
    readSomeAsync(
        buf,
        [this, buf, minimalSize, handler = std::move(handler), baseSize](
            SystemError::ErrorCode code, size_t size)
        {
            if (code != SystemError::noError || size == 0 ||
                buf->size() >= minimalSize)
            {
                return handler(code, buf->size() - baseSize);
            }

            readFixedAsync(buf, minimalSize, std::move(handler), baseSize);
        });
}

void AbstractCommunicatingSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    cancelIOAsync(nx::network::aio::EventType::etNone, std::move(handler));
}

void AbstractCommunicatingSocket::pleaseStopSync()
{
    cancelIOSync(nx::network::aio::EventType::etNone);
}


////////////////////////////////////////////////////////////
//// class AbstractDatagramSocket
////////////////////////////////////////////////////////////

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
