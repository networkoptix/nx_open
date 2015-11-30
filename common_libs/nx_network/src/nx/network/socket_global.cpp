#include "socket_global.h"

namespace nx {

SocketGlobals::SocketGlobals()
    : m_log( QnLog::logs() )
{
}

SocketGlobals::~SocketGlobals()
{
    m_addressPublisher.terminate();
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

