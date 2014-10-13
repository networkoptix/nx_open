/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_IMPL_H
#define SOCKET_IMPL_H

#include <array>
#include <atomic>

#include "aio/pollset.h"
#include "aio/aiothread.h"


class Socket;
namespace aio
{
    extern template class AIOThread<Socket>;
}

class SocketImpl
{
public:
    std::atomic<aio::AIOThread<Socket>*> aioThread;

    virtual ~SocketImpl() {}

    void*& getUserData( aio::EventType eventType )
    {
        return m_eventTypeToUserData[eventType];
    }

    void* const& getUserData( aio::EventType eventType ) const
    {
        return m_eventTypeToUserData[eventType];
    }

private:
    std::array<void*, aio::etMax> m_eventTypeToUserData;
};

#endif  //SOCKET_IMPL_H
