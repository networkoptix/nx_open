#ifndef NX_STUN_CC_MEDIATOR_CONNETION_H
#define NX_STUN_CC_MEDIATOR_CONNETION_H

#include "utils/network/stun/async_client.h"

namespace nx {
namespace stun {
namespace cc {

class MediatorConnection
{
public:
    MediatorConnection( const SocketAddress& address,
                        const String& systemId,
                        const String& serverId,
                        const String& authorizationKey );

    void updateAddresses( const std::list< SocketAddress >& addresses );

private:
    void updateExternalAddresses();
    void checkAddresses( const std::list< SocketAddress >& addresses );

private:
    stun::AsyncClient m_stunClient;
    const String m_systemId;
    const String m_serverId;
    const String m_authorizationKey;

    QnMutex m_mutex;
    std::set< SocketAddress > m_external;
    bool m_isExternalChanged;
    bool m_isExternalUpdateInProgress;
};

} // namespase cc
} // namespase stun
} // namespase nx

#endif // NX_STUN_CC_MEDIATOR_CONNETION_H
