#ifndef NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
#define NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H

#include "utils/network/stun/async_client.h"
#include "utils/network/cloud_connectivity/cdb_endpoint_fetcher.h"

namespace nx {
namespace cc {

class MediatorAddressPublisher
{
public:
    MediatorAddressPublisher( CloudModuleEndPointFetcher* addressFetcher,
                              String serverId );

    struct Authorization
    {
        String systemId;
        String key;

        bool operator == ( const Authorization& rhs ) const;
    };

    void authorizationChanged( boost::optional< Authorization > authorization );
    void updateAddresses( std::list< SocketAddress > addresses );

private:
    void updateExternalAddresses();
    void checkAddresses( const std::list< SocketAddress >& addresses );

private:
    const String m_serverId;

    QnMutex m_mutex;
    boost::optional< SocketAddress > m_address;
    boost::optional< Authorization > m_authorization;
    std::unique_ptr< stun::AsyncClient > m_stunClient;

    std::set< SocketAddress > m_external;
    bool m_isExternalChanged;
    bool m_isExternalUpdateInProgress;
};

} // namespase cc
} // namespase nx

#endif // NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
