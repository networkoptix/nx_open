/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#include "socket_impl.h"


SocketImpl::SocketImpl()
{
    m_eventTypeToUserData.resize( PollSet::etCount );
}

void*& SocketImpl::getUserData( PollSet::EventType eventType )
{
    return m_eventTypeToUserData[eventType];
}

void* const& SocketImpl::getUserData( PollSet::EventType eventType ) const
{
    return m_eventTypeToUserData[eventType];
}
