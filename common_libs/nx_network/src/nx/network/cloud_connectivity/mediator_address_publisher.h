#ifndef NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
#define NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H

#include <nx/network/stun/async_client.h>
#include <nx/network/cloud_connectivity/cdb_endpoint_fetcher.h>
#include "utils/common/timermanager.h"

namespace nx {
namespace cc {

class NX_NETWORK_API MediatorAddressPublisher
{
public:
    static const TimerDuration DEFAULT_UPDATE_INTERVAL;

    MediatorAddressPublisher( String serverId,
                              TimerDuration updateInterval = DEFAULT_UPDATE_INTERVAL );
    ~MediatorAddressPublisher();

    struct Authorization
    {
        String systemId;
        String key;

        bool operator == ( const Authorization& rhs ) const;
    };

    void authorizationChanged( boost::optional< Authorization > authorization );
    void updateAddresses( std::list< SocketAddress > addresses );

private:
    void setupUpdateTimer();
    bool isCloudReady();

    void pingReportedAddresses();
    void publishPingedAddresses();

private:
    const String m_serverId;
    const TimerDuration m_updateInterval;

    QnMutex m_mutex;
    TimerManager::TimerGuard m_timerGuard;
    bool isTerminating;

    boost::optional< Authorization > m_stunAuthorization;
    std::unique_ptr< stun::AsyncClient > m_stunClient;

    std::list< SocketAddress > m_reportedAddresses;
    std::list< SocketAddress > m_pingedAddresses;
    std::list< SocketAddress > m_publishedAddresses;
};

} // namespase cc
} // namespase nx

#endif // NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
