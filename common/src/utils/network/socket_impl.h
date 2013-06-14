/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_IMPL_H
#define SOCKET_IMPL_H

#include <vector>

#include "aio/pollset.h"


class SocketImpl
{
public:
    SocketImpl();

    void*& getUserData( PollSet::EventType eventType );
    void* const& getUserData( PollSet::EventType eventType ) const;

private:
    std::vector<void*> m_eventTypeToUserData;
};

#endif  //SOCKET_IMPL_H
