/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#ifndef NX_SOCKET_GLOBALS_HOLDER_H
#define NX_SOCKET_GLOBALS_HOLDER_H

#include <nx/network/socket_global.h>
#include <nx/utils/singleton.h>
#include <utils/common/cpp14.h>


class SocketGlobalsHolder
:
    public Singleton<SocketGlobalsHolder>
{
public:
    SocketGlobalsHolder()
    :
        m_socketGlobalsGuard(std::make_unique<nx::network::SocketGlobals::InitGuard>())
    {
    }

    void reinitialize()
    {
        m_socketGlobalsGuard.reset();
        m_socketGlobalsGuard = std::make_unique<nx::network::SocketGlobals::InitGuard>();
    }

private:
    std::unique_ptr<nx::network::SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

#endif  //NX_SOCKET_GLOBALS_HOLDER_H
