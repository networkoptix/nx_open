/**********************************************************
* 18 may 2015
* a.kolesnikov
***********************************************************/

#include "server_managers.h"


namespace nx_http
{
    ServerManagers::ServerManagers()
    :
        m_dispatcher( nullptr ),
        m_authenticationManager( nullptr )
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

    void ServerManagers::setAuthenticationManager( AbstractAuthenticationManager* const authManager )
    {
        m_authenticationManager = authManager;
    }

    AbstractAuthenticationManager* ServerManagers::authenticationManager()
    {
        return m_authenticationManager;
    }
}
