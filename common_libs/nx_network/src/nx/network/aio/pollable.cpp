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
    std::unique_ptr<CommonSocketImpl> impl )
:
    m_fd( fd ),
    m_impl( std::move(impl) ),
    m_readTimeoutMS( 0 ),
    m_writeTimeoutMS( 0 )
{
    SocketGlobals::check();
    if( !m_impl )
        m_impl.reset( new CommonSocketImpl() );
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

CommonSocketImpl* Pollable::impl()
{
    return m_impl.get();
}

const CommonSocketImpl* Pollable::impl() const
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
