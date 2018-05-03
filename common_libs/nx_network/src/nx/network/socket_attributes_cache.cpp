#include "socket_attributes_cache.h"

namespace nx {
namespace network {

namespace {

template<typename Value>
bool verifyAttribute(
    const AbstractSocket& socket,
    bool (AbstractSocket::*getAttributeFunc)(Value*) const,
    const boost::optional<Value>& expected)
{
    if (!expected)
        return true;

    Value actual{};
    if (!(socket.*getAttributeFunc)(&actual))
        return false;

    return *expected == actual;
}

} // namespace

bool verifySocketAttributes(
    const AbstractSocket& socket,
    const SocketAttributes& attributes)
{
    if (attributes.aioThread)
    {
        if (socket.getAioThread() != *attributes.aioThread)
            return false;
    }

    return verifyAttribute(socket, &AbstractSocket::getReuseAddrFlag, attributes.reuseAddrFlag)
        && verifyAttribute(socket, &AbstractSocket::getReusePortFlag, attributes.reusePortFlag)
        && verifyAttribute(socket, &AbstractSocket::getNonBlockingMode, attributes.nonBlockingMode)
        && verifyAttribute(socket, &AbstractSocket::getSendBufferSize, attributes.sendBufferSize)
        && verifyAttribute(socket, &AbstractSocket::getRecvBufferSize, attributes.recvBufferSize)
        && verifyAttribute(socket, &AbstractSocket::getRecvTimeout, attributes.recvTimeout)
        && verifyAttribute(socket, &AbstractSocket::getSendTimeout, attributes.sendTimeout);
}

} // namespace network
} // namespace nx
