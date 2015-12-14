#include "mediator_address_publisher.h"

#include "common/common_globals.h"
#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/cc/custom_stun.h>

namespace nx {
namespace cc {

const TimerDuration MediatorAddressPublisher::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 10 );

MediatorAddressPublisher::MediatorAddressPublisher()
    : m_updateInterval( DEFAULT_UPDATE_INTERVAL )
    , m_state( State::kInit )
    , m_timerSocket( SocketFactory::createStreamSocket() )
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

    m_timerSocket->registerTimer( m_updateInterval.count(), [ this ]()
    {
        QnMutexLocker lk( &m_mutex );
        pingReportedAddresses( &lk );
    } );
}

void MediatorAddressPublisher::pingReportedAddresses( QnMutexLockerBase* lk )
{
    if( m_state == State::kTerminated )
        return;

    if( !m_mediatorConnection )
        m_mediatorConnection = SocketGlobals::mediatorConnector().systemConnection();

    if( m_reportedAddresses == m_pingedAddresses )
        return publishPingedAddresses( lk );

    auto addresses = m_reportedAddresses;
    lk->unlock();
    m_mediatorConnection->ping( std::move(addresses),
                                [this](bool success, std::list<SocketAddress> addrs)
    {
        QnMutexLocker lk( &m_mutex );
        if( !success )
        {
            m_pingedAddresses.clear();
            m_publishedAddresses.clear();
            return setupUpdateTimer( &lk );
        }

        m_pingedAddresses = std::move(addrs);
        NX_LOGX( lm( "Pinged addresses: %1" )
                 .container( m_publishedAddresses ), cl_logDEBUG1 );

        publishPingedAddresses( &lk );
    } );
}

void MediatorAddressPublisher::publishPingedAddresses( QnMutexLockerBase* lk )
{
    if( m_state == State::kTerminated )
        return;

    if( !m_mediatorConnection )
        m_mediatorConnection = SocketGlobals::mediatorConnector().systemConnection();

    auto addresses = m_pingedAddresses;
    lk->unlock();
    m_mediatorConnection->bind( std::move(addresses), [this](bool success)
    {
        QnMutexLocker lk( &m_mutex );
        if( !success )
        {
            m_pingedAddresses.clear();
            m_publishedAddresses.clear();
            return setupUpdateTimer( &lk );
        }

        m_publishedAddresses = m_pingedAddresses;
        NX_LOGX( lm( "Published addresses: %1" )
                 .container( m_publishedAddresses ), cl_logDEBUG1 );

        setupUpdateTimer( &lk );
    } );
}

void MediatorAddressPublisher::pleaseStop(std::function<void()> handler)
{
    {
        QnMutexLocker lk( &m_mutex );
        m_state = State::kTerminated;
    }

    nx::BarrierHandler barrier( std::move(handler) );
    m_timerSocket->pleaseStop( barrier.fork() );

    QnMutexLocker lk( &m_mutex );
    if( m_mediatorConnection )
    {
        lk.unlock();
        m_mediatorConnection->pleaseStop( barrier.fork() );
    }
}

} // namespase cc
} // namespase nx
