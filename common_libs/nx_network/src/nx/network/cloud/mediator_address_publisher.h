#ifndef NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
#define NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H

#include <nx/utils/timer_manager.h>

#include "mediator_connections.h"
#include "nx/network/aio/timer.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API MediatorAddressPublisher
    : public QnStoppableAsync
{
public:
    MediatorAddressPublisher(
            std::shared_ptr< hpm::api::MediatorServerTcpConnection > mediatorConnection );

    static const TimerDuration DEFAULT_UPDATE_INTERVAL;

    /** Should be called before initial @fn updateAuthorization */
    void setUpdateInterval( TimerDuration updateInterval );
    void updateAddresses( std::list< SocketAddress > addresses );

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

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

    nx::network::aio::Timer m_timer;
    std::shared_ptr< hpm::api::MediatorServerTcpConnection > m_mediatorConnection;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_MEDIATOR_ADDRESS_PUBLISHER_H
