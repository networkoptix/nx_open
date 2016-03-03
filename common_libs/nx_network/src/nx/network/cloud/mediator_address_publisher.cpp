#include "mediator_address_publisher.h"

#include "common/common_globals.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

const TimerDuration MediatorAddressPublisher::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 10 );

MediatorAddressPublisher::MediatorAddressPublisher(
        std::shared_ptr< hpm::api::MediatorServerTcpConnection > mediatorConnection)
    : m_updateInterval( DEFAULT_UPDATE_INTERVAL )
    , m_state( State::kInit )
    , m_mediatorConnection( std::move(mediatorConnection) )
{
}

void MediatorAddressPublisher::setUpdateInterval( TimerDuration updateInterval )
{
    QnMutexLocker lk( &m_mutex );
    m_updateInterval = updateInterval;
}

void MediatorAddressPublisher::updateAddresses( std::list< SocketAddress > addresses )
{
    QnMutexLocker lk( &m_mutex );
    if( m_reportedAddresses == addresses )
        return;

    NX_LOGX( lm( "New addresses: %1" ).container( addresses ), cl_logDEBUG1 );
    m_reportedAddresses = std::move( addresses );

    if( m_state == State::kInit )
    {
        m_state = State::kProgress;
        pingReportedAddresses( &lk );
    }
}

void MediatorAddressPublisher::setupUpdateTimer( QnMutexLockerBase* /*lk*/ )
{
    if( m_state == State::kTerminated )
        return;

    m_timer.start(
        m_updateInterval,
        [this]()
        {
            QnMutexLocker lk( &m_mutex );
            pingReportedAddresses( &lk );
        });
}

void MediatorAddressPublisher::pingReportedAddresses( QnMutexLockerBase* lk )
{
    if( m_state == State::kTerminated )
        return;

    if( m_reportedAddresses == m_pingedAddresses )
        return publishPingedAddresses( lk );

    auto addresses = m_reportedAddresses;
    lk->unlock();
    m_mediatorConnection->ping(
        nx::hpm::api::PingRequest(std::move(addresses)),
        [this](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::PingResponse responseData)
        {
            QnMutexLocker lk( &m_mutex );

            if (resultCode != nx::hpm::api::ResultCode::ok)
            {
                m_pingedAddresses.clear();
                m_publishedAddresses.clear();
                return setupUpdateTimer( &lk );
            }

            m_pingedAddresses = std::move(responseData.endpoints);
            NX_LOGX( lm( "Pinged addresses: %1" )
                     .container( m_publishedAddresses ), cl_logDEBUG1 );

            publishPingedAddresses( &lk );
        });
}

void MediatorAddressPublisher::publishPingedAddresses( QnMutexLockerBase* lk )
{
    if( m_state == State::kTerminated )
        return;

    auto addresses = m_pingedAddresses;
    lk->unlock();
    m_mediatorConnection->bind(
        nx::hpm::api::BindRequest(std::move(addresses)),
        [this](nx::hpm::api::ResultCode resultCode)
        {
            QnMutexLocker lk( &m_mutex );
            if (resultCode != nx::hpm::api::ResultCode::ok)
            {
                m_pingedAddresses.clear();
                m_publishedAddresses.clear();
                return setupUpdateTimer( &lk );
            }

            m_publishedAddresses = m_pingedAddresses;
            NX_LOGX( lm( "Published addresses: %1" )
                     .container( m_publishedAddresses ), cl_logDEBUG1 );

            setupUpdateTimer( &lk );
        });
}

void MediatorAddressPublisher::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    {
        QnMutexLocker lk( &m_mutex );
        m_state = State::kTerminated;
    }

    nx::BarrierHandler barrier( std::move(handler) );
    m_timer.pleaseStop( barrier.fork() );

    QnMutexLocker lk( &m_mutex );
    if( m_mediatorConnection )
    {
        lk.unlock();
        m_mediatorConnection->pleaseStop( barrier.fork() );
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
