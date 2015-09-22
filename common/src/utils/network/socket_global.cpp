#include "socket_global.h"

namespace nx {

SocketGlobals::SocketGlobals()
    : m_log( QnLog::logs() )
{
}

SocketGlobals& SocketGlobals::instance()
{
    std::call_once( s_onceFlag, [](){ s_instance.reset( new SocketGlobals ); } );
    return *s_instance;
}

std::once_flag SocketGlobals::s_onceFlag;
std::unique_ptr< SocketGlobals > SocketGlobals::s_instance;

} // namespace nx

