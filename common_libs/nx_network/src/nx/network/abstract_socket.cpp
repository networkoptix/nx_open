
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
