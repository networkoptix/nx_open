/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#include "system_socket_impl.h"


SocketImpl::SocketImpl()
{
    m_eventTypeToUserData.resize( aio::etMax );
}

void*& SocketImpl::getUserData( aio::EventType eventType )
{
    return m_eventTypeToUserData[eventType];
}

void* const& SocketImpl::getUserData( aio::EventType eventType ) const
{
    return m_eventTypeToUserData[eventType];
}
