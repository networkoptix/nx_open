#include "socket_global.h"

namespace nx {

SocketGlobals::SocketGlobals()
    : m_log( QnLog::logs() )
{
}

SocketGlobals::~SocketGlobals()
{
    std::promise< void > promise;
    BarrierHandler barrier([&](){ promise.set_value(); });

    m_addressResolver.pleaseStop( barrier.fork() );
    m_addressPublisher.pleaseStop( barrier.fork() );
    m_mediatorConnector.pleaseStop( barrier.fork() );

    promise.get_future().wait();
}

void SocketGlobals::init()
{
	QnMutexLocker lk( &s_mutex );
	if( ++s_counter == 1 )
        s_instance = new SocketGlobals;
}

void SocketGlobals::deinit()
{
	QnMutexLocker lk( &s_mutex );
    if( --s_counter == 0 )
        delete s_instance;
}

QnMutex SocketGlobals::s_mutex;
size_t SocketGlobals::s_counter( 0 );
SocketGlobals* SocketGlobals::s_instance;

} // namespace nx

