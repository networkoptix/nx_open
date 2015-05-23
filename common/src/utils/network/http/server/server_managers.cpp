/**********************************************************
* 18 may 2015
* a.kolesnikov
***********************************************************/

#include "server_managers.h"


namespace nx_http
{
    ServerManagers::ServerManagers()
    :
        m_dispatcher( nullptr )
    {
    }

    void ServerManagers::setDispatcher( MessageDispatcher* const dispatcher )
    {
        m_dispatcher = dispatcher;
    }

    MessageDispatcher* ServerManagers::dispatcher()
    {
        return m_dispatcher;
    }
}
