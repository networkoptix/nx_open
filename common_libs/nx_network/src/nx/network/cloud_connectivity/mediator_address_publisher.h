#ifndef NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
#define NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H

#include <nx/network/stun/async_client.h>
#include <nx/network/cloud_connectivity/cdb_endpoint_fetcher.h>
#include <nx/utils/timermanager.h>

namespace nx {
    class SocketGlobals;
}   //nx

namespace nx {
namespace cc {

class NX_NETWORK_API MediatorAddressPublisher
{
    MediatorAddressPublisher();
    friend class ::nx::SocketGlobals;

public:
    static const TimerDuration DEFAULT_UPDATE_INTERVAL;
    struct Authorization
    {
        String serverId;
        String systemId;
        String key;

        bool operator == ( const Authorization& rhs ) const;
    };

    /** Should be called before initial @fn updateAuthorization */
    void setUpdateInterval( TimerDuration updateInterval );
    void updateAuthorization( boost::optional< Authorization > authorization );
    void updateAddresses( std::list< SocketAddress > addresses );

private:
    void setupUpdateTimer();
    bool isCloudReady();
    void pingReportedAddresses();
    void publishPingedAddresses();
    void terminate();

private:
    QnMutex m_mutex;
    TimerDuration m_updateInterval;
    bool isTerminating;
    std::unique_ptr< AbstractStreamSocket > m_timerSocket;

    boost::optional< Authorization > m_stunAuthorization;
    stun::AsyncClient* m_stunClient;

    std::list< SocketAddress > m_reportedAddresses;
    std::list< SocketAddress > m_pingedAddresses;
    std::list< SocketAddress > m_publishedAddresses;
};

} // namespase cc
} // namespase nx

#endif // NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
