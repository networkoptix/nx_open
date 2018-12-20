#include "pollable.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {

Pollable::Pollable(
    AbstractSocket::SOCKET_HANDLE fd,
    std::unique_ptr<CommonSocketImpl> impl)
    :
    m_fd(fd),
    m_impl(std::move(impl)),
    m_readTimeoutMS(0),
    m_writeTimeoutMS(0)
{
    SocketGlobals::verifyInitialization();
    if (!m_impl)
        m_impl.reset(new CommonSocketImpl());
}

AbstractSocket::SOCKET_HANDLE Pollable::handle() const
{
    return m_fd;
}

AbstractSocket::SOCKET_HANDLE Pollable::takeHandle()
{
    auto systemHandle = m_fd;
    m_fd = -1;
    return systemHandle;
}

bool Pollable::getRecvTimeout(unsigned int* millis) const
{
    *millis = m_readTimeoutMS;
    return true;
}

bool Pollable::getSendTimeout(unsigned int* millis) const
{
    *millis = m_writeTimeoutMS;
    return true;
}

CommonSocketImpl* Pollable::impl()
{
    return m_impl.get();
}

const CommonSocketImpl* Pollable::impl() const
{
    return m_impl.get();
}

bool Pollable::getLastError(SystemError::ErrorCode* /*errorCode*/) const
{
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

nx::network::aio::AbstractAioThread* Pollable::getAioThread() const
{
    return nx::network::SocketGlobals::aioService().getSocketAioThread(
        const_cast<Pollable*>(this));   //TODO #ak deal with this const_cast
}

void Pollable::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    nx::network::SocketGlobals::aioService().bindSocketToAioThread(this, aioThread);
}

bool Pollable::isInSelfAioThread() const
{
    return m_impl->aioThread.load() == QThread::currentThread();
}

} // namespace network
} // namespace nx
