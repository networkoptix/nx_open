#ifndef NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
#define NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H

#include <nx/utils/timermanager.h>

#include "mediator_connections.h"

namespace nx {
    class SocketGlobals;
}   //nx

namespace nx {
namespace cc {

class NX_NETWORK_API MediatorAddressPublisher
    : public QnStoppableAsync
{
    MediatorAddressPublisher();
    friend class ::nx::SocketGlobals;

public:
    static const TimerDuration DEFAULT_UPDATE_INTERVAL;

    /** Should be called before initial @fn updateAuthorization */
    void setUpdateInterval( TimerDuration updateInterval );
    void updateAddresses( std::list< SocketAddress > addresses );

    void pleaseStop( std::function<void()> handler ) override;

private:
    void setupUpdateTimer( QnMutexLockerBase* lk );
    void pingReportedAddresses( QnMutexLockerBase* lk );
    void publishPingedAddresses( QnMutexLockerBase* lk );

private:
    QnMutex m_mutex;
    TimerDuration m_updateInterval;
    enum class State { kInit, kProgress, kTerminated } m_state;

    std::list< SocketAddress > m_reportedAddresses;
    std::list< SocketAddress > m_pingedAddresses;
    std::list< SocketAddress > m_publishedAddresses;

    std::unique_ptr< AbstractStreamSocket > m_timerSocket;
    std::shared_ptr< MediatorSystemConnection > m_mediatorConnection;
};

} // namespase cc
} // namespase nx

#endif // NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
