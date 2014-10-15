/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#include "system_socket_impl.h"


SocketImpl::SocketImpl()
:
    aioThread( nullptr ),
    terminated( false )
{
}
