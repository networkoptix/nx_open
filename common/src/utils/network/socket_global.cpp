#include "socket_global.h"

namespace nx {

SocketGlobals& SocketGlobals::instance()
{
    if( s_instance )
        return *s_instance;

    QnMutexLocker lk( &s_mutex );
    if( !s_instance )
        s_instance.reset( new SocketGlobals() );

    return *s_instance;
}

QnMutex SocketGlobals::s_mutex;
std::unique_ptr< SocketGlobals > SocketGlobals::s_instance;

} // namespace nx

