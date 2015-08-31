#include "upnp_port_mapper.h"

#include "common/common_globals.h"
#include "utils/common/log.h"

#include <random>

static const size_t BREAK_FAULTS_COUNT = 5; // faults in a row
static const size_t BREAK_TIME_PER_FAULT = 1 * 60; // wait 1 minute per fault in a row
static const size_t BREAK_TIME_MAX = 2 * 60 * 60; // dont wait more than 2 hours

static const quint16 PORT_SAFE_RANGE_BEGIN = 4096; // begining of range to peak rnd port
static const quint16 PORT_SAFE_RANGE_END = 49151;

static const quint16 MAPPING_TIME_RATIO = 10; // 10 times longer then we check

namespace nx_upnp {

PortMapper::PortMapper( quint64 checkMappingsInterval,
                        const QString& description, const QString& device )
    : SearchAutoHandler( device )
    , m_upnpClient( new AsyncClient() )
    , m_description( description )
    , m_checkMappingsInterval( checkMappingsInterval )
{
    m_timerId = TimerManager::instance()->addTimer( this, m_checkMappingsInterval );
}

PortMapper::~PortMapper()
{
    quint64 timerId;
    {
        QMutexLocker lock( &m_mutex );
        timerId = m_timerId;
        m_timerId = 0;
    }

    TimerManager::instance()->joinAndDeleteTimer( timerId );
    m_upnpClient.reset();
}

const quint64 PortMapper::DEFAULT_CHECK_MAPPINGS_INTERVAL = 10 * 60 * 1000; // 10 min

bool PortMapper::enableMapping( quint16 port, Protocol protocol,
                                std::function< void( SocketAddress ) > callback )
{
    PortId portId( port, protocol );

    QMutexLocker lock( &m_mutex );
    if( !m_mapRequests.emplace( std::move( portId ), std::move( callback ) ).second )
        return false; // port already mapped

    // ask to map this port on all known devices
    for( auto& device : m_devices )
        ensureMapping( device.second, port, protocol );

    return true;
}

bool PortMapper::disableMapping( quint16 port, Protocol protocol )
{
    QMutexLocker lock( &m_mutex );
    const auto it = m_mapRequests.find( PortId( port, protocol ) );
    if( it == m_mapRequests.end() )
        return false;

    for( auto& device : m_devices )
    {
        const auto mapping = device.second.mapped.find( it->first );
        if( mapping != device.second.mapped.end() )
            m_upnpClient->deleteMapping(
                device.second.url, mapping->second, mapping->first.protocol,
                []( bool ){ /* TODO: think what can we do */ } );
    }

    m_mapRequests.erase( it );
    return true;
}

PortMapper::FailCounter::FailCounter()
    : m_failsInARow( 0 )
    , m_lastFail( 0 )
{
}

void PortMapper::FailCounter::success()
{
    m_failsInARow = 0;
}

void PortMapper::FailCounter::failure()
{
    ++m_failsInARow;
    m_lastFail = QDateTime::currentDateTime().toTime_t();
}

bool PortMapper::FailCounter::isOk()
{
    if( m_failsInARow < BREAK_FAULTS_COUNT )
        return true;

    const auto breakTime = m_lastFail + BREAK_TIME_PER_FAULT * m_failsInARow;
    return QDateTime::currentDateTime().toTime_t() > breakTime;
}

PortMapper::PortId::PortId( quint16 port_, Protocol protocol_ )
    : port( port_ ), protocol( protocol_ )
{
}

bool PortMapper::PortId::operator < ( const PortId& rhs ) const
{
    if( port < rhs.port ) return true;
    if( port == rhs.port && protocol < rhs.protocol ) return true;
    return false;
}

bool PortMapper::processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const DeviceInfo& devInfo, const QByteArray& xmlDevInfo )
{
    static_cast< void >( xmlDevInfo );
    return searchForMappers( HostAddress( localAddress.toString() ), devAddress, devInfo );
}

bool PortMapper::searchForMappers( const HostAddress& localAddress,
                                   const SocketAddress& devAddress,
                                   const DeviceInfo& devInfo )
{
    bool atLeastOneFound = false;
    for( const auto& service : devInfo.serviceList)
    {
        if( service.serviceType != AsyncClient::WAN_IP )
            continue; // uninteresting

        QUrl url;
        url.setHost( devAddress.address.toString() );
        url.setPort( devAddress.port );
        url.setPath( service.controlUrl );

        QMutexLocker lk( &m_mutex );
        addNewDevice( localAddress, url, devInfo.serialNumber );
    }

    for( const auto& subDev : devInfo.deviceList )
        atLeastOneFound |= searchForMappers( localAddress, devAddress, subDev );

    return atLeastOneFound;
}

void PortMapper::onTimer( const quint64& /*timerID*/ )
{
    QMutexLocker lock( &m_mutex );
    for( auto& device : m_devices )
    {
        updateExternalIp( device.second );
        for( const auto& mapping : device.second.mapped )
            checkMapping( device.second, mapping.first.port,
                          mapping.second, mapping.first.protocol );
    }

    if( m_timerId )
        m_timerId = TimerManager::instance()->addTimer( this, m_checkMappingsInterval );
}

void PortMapper::addNewDevice( const HostAddress& localAddress,
                               const QUrl& url, const QString& serial )
{
    const auto itBool = m_devices.emplace( serial, Device() );
    if( !itBool.second )
        return; // known device

    Device& newDevice = itBool.first->second;
    newDevice.internalIp = localAddress;
    newDevice.url = url;

    updateExternalIp( newDevice );
    for( const auto map : m_mapRequests )
        ensureMapping( newDevice, map.first.port, map.first.protocol );

    NX_LOG( lit( "%1 New device %2 ( %3 ) has been found on %4" )
            .arg( QLatin1String( Q_FUNC_INFO ) ).arg( url.toString() )
            .arg( serial ).arg( localAddress.toString() ), cl_logDEBUG2 );
}

void PortMapper::updateExternalIp( Device& device )
{
    if( !device.failCounter.isOk() )
        return;

    const auto result = m_upnpClient->externalIp(
        device.url, [ this, &device ]( HostAddress externalIp )
    {
        std::list< Guard > callbackGuards;
        {
            QMutexLocker lock( &m_mutex );
            NX_LOG( lit( "%1 externalIp='%2' on device %3" )
                    .arg( QLatin1String( Q_FUNC_INFO ) )
                    .arg( externalIp.toString() )
                    .arg( device.url.toString() ), cl_logDEBUG1 );

            if( externalIp == HostAddress() )
            {
                device.failCounter.failure();
                return;
            }

            device.failCounter.success();
            if( device.externalIp != externalIp )
                callbackGuards = changeIpEvents( device, std::move(externalIp) );
        }
    });

    if( !result )
        device.failCounter.failure();
}

// TODO: reduse the size of method
void PortMapper::checkMapping( Device& device, quint16 inPort, quint16 exPort,
                               Protocol protocol )
{
    if( !device.failCounter.isOk() )
        return;

    const auto result = m_upnpClient->getMapping(
        device.url, exPort, protocol,
        [ this, &device, inPort, exPort, protocol ]( AsyncClient::MappingInfo info )
    {
        QMutexLocker lk( &m_mutex );
        device.failCounter.success();

        if( info.internalPort == inPort &&
            info.externalPort == exPort &&
            info.protocol == protocol &&
            info.internalIp == device.internalIp )
        {
            return; // all ok
        }

        // save and report, that mapping has gone
        device.mapped.erase( device.mapped.find( PortId( inPort, protocol ) ) );
        const auto request = m_mapRequests.find( PortId( inPort, protocol ) );
        if( request == m_mapRequests.end() )
        {
            // no need to provide this mapping any longer
            return;
        }

        SocketAddress invalid( device.externalIp, 0 );
        const auto callback = request->second;

        // try to map over again (in case of router reboot)
        ensureMapping( device, inPort, protocol );

        lk.unlock();
        callback( invalid );
    } );

    if( !result )
        device.failCounter.failure();
}

// TODO: reduse the size of method
void PortMapper::ensureMapping( Device& device, quint16 inPort, Protocol protocol )
{
    if( !device.failCounter.isOk() )
        return;

    const bool result = m_upnpClient->getAllMappings( device.url,
        [ this, &device, inPort, protocol ]( AsyncClient::MappingList list )
    {
        QMutexLocker lk( &m_mutex );
        device.failCounter.success();

        // Save existing mappings in case if router can silently remap them
        device.engagedPorts.clear();
        for( const auto mapping : list )
            device.engagedPorts.insert( PortId( mapping.externalPort, mapping.protocol ) );

        const auto deviceMap = device.mapped.find( PortId( inPort, protocol ) );
        const auto request = m_mapRequests.find( PortId( inPort, protocol ) );
        if( request == m_mapRequests.end() )
        {
            // no need to provide this mapping any longer
            return;
        }

        const auto callback = request->second;
        for( const auto mapping : list )
            if( mapping.internalIp == device.internalIp &&
                mapping.internalPort == inPort &&
                mapping.protocol == protocol &&
                ( mapping.duration == 0 || mapping.duration > m_checkMappingsInterval ) )
            {
                NX_LOG( lit( "%1 Already mapped %2" )
                        .arg( QLatin1String( Q_FUNC_INFO ) )
                        .arg( mapping.toString() ), cl_logDEBUG1 );

                if( deviceMap == device.mapped.end() ||
                    deviceMap->second != mapping.externalPort )
                {
                    // the change should be saved and reported
                    device.mapped[ PortId( inPort, protocol ) ] = mapping.externalPort;

                    lk.unlock();
                    callback( SocketAddress( device.externalIp, mapping.externalPort ) );
                }

                return;
            }

        // to ensure mapping we should create it from scratch
        makeMapping( device, inPort, protocol );

        if( deviceMap != device.mapped.end() )
        {
            SocketAddress invalid( device.externalIp, 0 );

            // address lose should be saved and reported
            device.mapped.erase( deviceMap );

            lk.unlock();
            callback( invalid );
        }
    } );

    if( !result )
        device.failCounter.failure();
}

// TODO: move into function below when supported
static std::default_random_engine randomEngine(
        std::chrono::system_clock::now().time_since_epoch().count() );
static std::uniform_int_distribution<quint16> portDistribution(
        PORT_SAFE_RANGE_BEGIN, PORT_SAFE_RANGE_END );

// TODO: reduse the size of method
void PortMapper::makeMapping( Device& device, quint16 inPort,
                              Protocol protocol, size_t retries )
{
    if( !device.failCounter.isOk() )
        return;

    quint16 desiredPort;
    do
        desiredPort = portDistribution( randomEngine );
    while( device.engagedPorts.count( PortId( desiredPort, protocol ) ) );

    const bool result = m_upnpClient->addMapping(
        device.url, device.internalIp, inPort, desiredPort, protocol, m_description,
        m_checkMappingsInterval * MAPPING_TIME_RATIO,
        [ this, &device, inPort, desiredPort, protocol, retries ]( bool success )
    {
        QMutexLocker lk( &m_mutex );
        if( !success )
        {
            device.failCounter.failure();

            if( retries > 0 )
                makeMapping( device, inPort, protocol, retries - 1 );
            else
                NX_LOG( lit( "%1 Cound not forward any port on %2" )
                        .arg( QLatin1String( Q_FUNC_INFO ) )
                        .arg( device.url.toString() ), cl_logERROR );
            return;
        }

        device.failCounter.success();

        // save successful mapping and call callback
        const auto request = m_mapRequests.find( PortId( inPort, protocol ) );
        if( request == m_mapRequests.end() )
        {
            // no need to provide this mapping any longer
            return;
        }

        const auto callback = request->second;
        device.mapped[ PortId( inPort, protocol ) ] = desiredPort;
        device.engagedPorts.insert( PortId( desiredPort, protocol ) );

        lk.unlock();
        callback( SocketAddress( device.externalIp, desiredPort ) );
    } );

    if( !result )
        device.failCounter.failure();
}

std::list< Guard > PortMapper::changeIpEvents( Device& device, HostAddress externalIp )
{
    std::list< Guard > callbackGuards;

    std::swap( device.externalIp, externalIp );
    for( auto& map : device.mapped )
    {
        const auto it = m_mapRequests.find( map.first );
        if( it != m_mapRequests.end() )
        {
            if( externalIp != HostAddress() )
            {
                // all mappings with old ip are not valid
                callbackGuards.push_back( Guard( std::bind(
                    it->second, SocketAddress( externalIp, 0 ) ) ) );
            }

            callbackGuards.push_back( Guard( std::bind(
                it->second, SocketAddress( device.externalIp, map.second ) ) ) );
        }
    }

    return callbackGuards;
}

} // namespace nx_upnp
