/**********************************************************
* 29 oct 2014
* a.kolesnikov
***********************************************************/

#include "pollable.h"

#include <utils/common/systemerror.h>


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

AbstractSocket::SOCKET_HANDLE Pollable::handle() const
{
    return m_fd;
}

bool Pollable::getRecvTimeout( unsigned int* millis )
{
    *millis = m_readTimeoutMS;
    return true;
}

bool Pollable::getSendTimeout( unsigned int* millis )
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

bool Pollable::getLastError( SystemError::ErrorCode* errorCode ) const
{
    SystemError::setLastErrorCode( SystemError::notImplemented );
    return false;
}
