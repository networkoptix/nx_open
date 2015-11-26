#include "socket_global.h"

namespace nx {

SocketGlobals::SocketGlobals()
    : m_log( QnLog::logs() )
{
}

void SocketGlobals::init()
{
	QnMutexLocker lk( &s_mutex );
	if( ++s_counter == 1 )
		s_instance.reset( new SocketGlobals );
}

void SocketGlobals::deinit()
{
	QnMutexLocker lk( &s_mutex );
	if( --s_counter == 0 )
		s_instance.reset();
}

QnMutex SocketGlobals::s_mutex;
size_t SocketGlobals::s_counter( 0 );
std::unique_ptr< SocketGlobals > SocketGlobals::s_instance;

} // namespace nx

