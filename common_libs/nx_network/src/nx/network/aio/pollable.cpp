/**********************************************************
* 29 oct 2014
* a.kolesnikov
***********************************************************/

#include "pollable.h"

#include <utils/common/systemerror.h>
#include <nx/network/socket_global.h>


namespace nx {
namespace network {

Pollable::Pollable(
    AbstractSocket::SOCKET_HANDLE fd,
    std::unique_ptr<PollableImpl> impl )
:
    m_fd( fd ),
    m_impl( std::move(impl) ),
    m_readTimeoutMS( 0 ),
    m_writeTimeoutMS( 0 )
{
    if( !m_impl )
        m_impl.reset( new PollableImpl() );
}

Pollable::Pollable(Pollable&& rhs)
:
    m_fd(std::move(rhs.m_fd)),
    m_impl(std::move(rhs.m_impl)),
    m_readTimeoutMS(std::move(rhs.m_readTimeoutMS)),
    m_writeTimeoutMS(std::move(rhs.m_writeTimeoutMS))
{
    rhs.m_fd = -1;
    rhs.m_readTimeoutMS = 0;
    rhs.m_writeTimeoutMS = 0;
}

Pollable& Pollable::operator=(Pollable&& rhs)
{
    if (&rhs == this)
        return *this;

    m_fd = std::move(rhs.m_fd);
    rhs.m_fd = -1;

    m_impl = std::move(rhs.m_impl);

    m_readTimeoutMS = std::move(rhs.m_readTimeoutMS);
    rhs.m_readTimeoutMS = 0;

    m_writeTimeoutMS = std::move(rhs.m_writeTimeoutMS);
    rhs.m_readTimeoutMS = 0;

    return *this;
}

AbstractSocket::SOCKET_HANDLE Pollable::handle() const
{
    return m_fd;
}

bool Pollable::getRecvTimeout( unsigned int* millis ) const
{
    *millis = m_readTimeoutMS;
    return true;
}

bool Pollable::getSendTimeout( unsigned int* millis ) const
{
    *millis = m_writeTimeoutMS;
    return true;
}

PollableImpl* Pollable::impl()
{
    return m_impl.get();
}

const PollableImpl* Pollable::impl() const
{
    return m_impl.get();
}

bool Pollable::getLastError( SystemError::ErrorCode* /*errorCode*/ ) const
{
    SystemError::setLastErrorCode( SystemError::notImplemented );
    return false;
}

nx::network::aio::AbstractAioThread* Pollable::getAioThread()
{
    return nx::network::SocketGlobals::aioService().getSocketAioThread(this);
}

void Pollable::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    nx::network::SocketGlobals::aioService().bindSocketToAioThread(this, aioThread);
}

}   //network
}   //nx
