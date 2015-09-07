#ifndef NX_STUN_CC_MEDIATOR_CONNETION_H
#define NX_STUN_CC_MEDIATOR_CONNETION_H

#include "utils/network/stun/async_client.h"

namespace nx {
namespace stun {
namespace cc {

class MediatorConnection
{
public:
    MediatorConnection();

    bool ping( const std::list< SocketAddress >& endpoints,
               std::function< void( std::list< SocketAddress > ) > handler );

    bool bind( const std::list< SocketAddress >& endpoints,
               std::function< void( bool ) > handler );

    bool listen( /* TODO: connection handler for hole punching */ );

    bool connect( const nx::String& hostName,
                  std::function< std::list< SocketAddress > > handler
                  /* TODO: hole punching handler  */ );

private:
    stun::AsyncClient m_stunClient;
};

} // namespase cc
} // namespase stun
} // namespase nx

#endif // NX_STUN_CC_MEDIATOR_CONNETION_H
