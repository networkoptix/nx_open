#include "upnp_port_mapper_internal.h"

#include "utils/common/model_functions.h"

#include <QDateTime>

static const size_t BREAK_FAULTS_COUNT = 5; // faults in a row
static const size_t BREAK_TIME_PER_FAULT = 1 * 60; // wait 1 minute per fault in a row
static const size_t BREAK_TIME_MAX = 2 * 60 * 60; // dont wait more than 2 hours

static const quint16 PORT_SAFE_RANGE_BEGIN = 4096; // begining of range to peak rnd port
static const quint16 PORT_SAFE_RANGE_END = 49151;
static const quint16 PORT_RETRY_COUNT = 5; // maximal number of ports to try open

namespace nx_upnp {

PortMapper::Callback::Callback(
        const std::function< void( const MappingInfo& ) >& callback )
    : m_callback( callback )
{
}

void PortMapper::Callback::call( const MappingInfo& info )
{
    QMutexLocker lk( &m_mutex );
    if( !m_callback || (
                m_state.externalIp == info.externalIp &&
                m_state.externalPort == info.externalPort ) )
        return;

    m_state = info;
    m_callback( info );
}

void PortMapper::Callback::clear()
{
    QMutexLocker lk( &m_mutex );
    m_callback = nullptr;
}

FailCounter::FailCounter()
    : m_failsInARow( 0 )
    , m_lastFail( 0 )
{
}

void FailCounter::success()
{
    m_failsInARow = 0;
}

void FailCounter::failure()
{
    ++m_failsInARow;
    m_lastFail = QDateTime::currentDateTime().toTime_t();
}

bool FailCounter::isOk()
{
    if( m_failsInARow < BREAK_FAULTS_COUNT )
        return true;

    const auto breakTime = m_lastFail + BREAK_TIME_PER_FAULT * m_failsInARow;
    return QDateTime::currentDateTime().toTime_t() > breakTime;
}

PortMapper::Device::Device( AsyncClient* upnpClient,
                           const HostAddress& internalIp,
                           const QUrl& url,
                           const QString& description )
    : m_upnpClient( upnpClient )
    , m_internalIp( internalIp )
    , m_url( url )
    , m_description( description )
{
    m_upnpClient->externalIp( url, [this]( const HostAddress& newIp )
    {
        std::vector< std::function< void() > > successQueue;
        {
            QMutexLocker lk( &m_mutex );
            m_externalIp = newIp;
            std::swap( m_successQueue, successQueue );
        }

        // notify all callbacks in queue (they were not notified bc externalIp was unknown
        for( const auto callback : successQueue )
            callback();
    } );
}

HostAddress PortMapper::Device::externalIp() const
{
    QMutexLocker lk( &m_mutex );
    return m_externalIp;
}

bool PortMapper::Device::map(
        quint16 port, quint16 desiredPort, Protocol protocol,
        const std::function< void( quint16 ) >& callback )
{
    QMutexLocker lk( &m_mutex );
    if( !m_faultCounter.isOk() )
        return false;

    const bool result = m_upnpClient->getAllMappings( m_url,
        [ this, port, desiredPort, protocol, callback ]
            ( const AsyncClient::MappingList& list )
    {
        QMutexLocker lk( &m_mutex );
        for( const auto mapping : list )
            if( mapping.internalIp == m_internalIp &&
                mapping.internalPort == port &&
                mapping.protocol == protocol )
            {
                m_faultCounter.success();
                m_alreadyMapped[ std::make_pair( port, protocol ) ]
                        = mapping.externalPort;
                lk.unlock();

                // already mapped, let's use this one
                callback( mapping.externalPort );
                return;
            }

        if( !mapImpl( port, desiredPort, protocol, PORT_RETRY_COUNT, callback ) )
        {
            m_faultCounter.failure();
            lk.unlock();

            callback( 0 ); // mapping failed
        }
    } );

    if( !result )
        m_faultCounter.failure();

    return result;
}

bool PortMapper::Device::unmap( quint16 port, Protocol protocol,
                                const std::function< void() >& callback )
{
    QMutexLocker lk( &m_mutex );
    const auto it = m_alreadyMapped.find( std::make_pair( port, protocol ) );
    if( it == m_alreadyMapped.end() )
        return false;

    const auto external = it->second;
    return m_upnpClient->deleteMapping( m_url, external, protocol,
                                        [ this, callback, port, protocol ]( bool success )
    {
        if( success )
        {
            QMutexLocker lk( &m_mutex );
            m_alreadyMapped.erase( std::make_pair( port, protocol ) );
        }

        // if something goes wrong the mapping will be avaliable when
        // server starts next time
        callback();
    } );
}

bool PortMapper::Device::check( quint16 port, Protocol protocol,
                                const std::function< void( quint16 ) >& callback )
{
    QMutexLocker lk( &m_mutex );
    const auto it = m_alreadyMapped.find( std::make_pair( port, protocol ) );
    if( it == m_alreadyMapped.end() )
        return false;

    const auto external = it->second;
    return m_upnpClient->getMapping(
        m_url, external, protocol,
        [ this, port, external, protocol, callback ]( const AsyncClient::MappingInfo& info )
    {
        QMutexLocker lk( &m_mutex );
        if( info.internalPort == port &&
            info.internalIp == m_internalIp )
        {
            lk.unlock();
            callback( info.externalPort );
            return;
        }

        // looks like we lost this one, try to restore
        m_alreadyMapped.erase( std::make_pair( port, protocol ) );
        if( !mapImpl( port, external, protocol, PORT_RETRY_COUNT, callback ) )
        {
            lk.unlock();
            callback( 0 );
        }
    } );
}

bool PortMapper::Device::mapImpl(
        quint16 port, quint16 desiredPort, Protocol protocol, size_t retrys,
        const std::function< void( quint16 ) >& callback )
{
    if( !m_faultCounter.isOk() )
        return false;

    const bool result = m_upnpClient->addMapping(
        m_url, m_internalIp, port, desiredPort, protocol, m_description,
        [ this, callback, port, desiredPort, protocol, retrys ]( bool success )
    {
        QMutexLocker lk( &m_mutex );
        if( !success )
        {
            if( retrys ) // try to forward different port number
            {
                const auto step = qrand() % (PORT_SAFE_RANGE_END - PORT_SAFE_RANGE_BEGIN);
                const auto newPort = PORT_SAFE_RANGE_BEGIN + step;

                mapImpl( port, newPort, protocol, retrys - 1, callback );
                return;
            }

            m_faultCounter.failure();
            lk.unlock();

            callback( 0 );
            return;
        }

        m_faultCounter.success();
        if( m_externalIp == HostAddress::anyHost )
        {
            // will be notified as soon as we know external IP
            m_successQueue[ port ] = [ this, port, protocol, desiredPort, callback ]()
            {
                {
                    QMutexLocker lk( &m_mutex );
                    m_alreadyMapped[ std::make_pair( port, protocol ) ] = desiredPort;
                }
                callback( desiredPort );
            };
            return;
        }

        m_alreadyMapped[ std::make_pair( port, protocol ) ] = desiredPort;
        lk.unlock();

        callback( desiredPort );
    } );


    if( !result )
        m_faultCounter.failure();

    return result;
}

} // namespace nx_upnp
