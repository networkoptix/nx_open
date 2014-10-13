/**********************************************************
* 13 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef COMMON_SOCKET_IMPL_H
#define COMMON_SOCKET_IMPL_H

#include <array>
#include <atomic>

#include "aio/pollset.h"
#include "aio/aiothread.h"


template<class SocketType>
class CommonSocketImpl
{
public:
    std::atomic<aio::AIOThread<SocketType>*> aioThread;

    virtual ~CommonSocketImpl() {}

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

#endif  //COMMON_SOCKET_IMPL_H
