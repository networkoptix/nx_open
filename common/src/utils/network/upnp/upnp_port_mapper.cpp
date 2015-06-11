#include "upnp_port_mapper.h"

#include "upnp_port_mapper_internal.h"

namespace nx_upnp {

PortMapper::PortMapper( quint64 checkMappingsInterval,
                        const QString& description, const QString& device )
    : SearchAutoHandler( device )
    , m_upnpClient( new AsyncClient() )
    , m_asyncInProgress( 0 )
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

    QMutexLocker lock( &m_mutex );
    while( m_asyncInProgress )
        m_asyncCondition.wait( &m_mutex );
}

const quint64 PortMapper::DEFAULT_CHECK_MAPPINGS_INTERVAL = 10 * 60 * 1000; // 10 min

PortMapper::MappingInfo::MappingInfo( quint16 inPort, quint16 exPort,
                                      const HostAddress& exIp, Protocol prot )
    : internalPort( inPort ), externalPort( exPort )
    , externalIp( exIp ), protocol( prot )
{
}

bool PortMapper::enableMapping(
        quint16 port, Protocol protocol,
        std::function< void( const MappingInfo& ) > callback )
{
    const auto request = std::make_pair( port, protocol );

    QMutexLocker lock( &m_mutex );
    if( !m_mappings.emplace( request, std::make_shared< Callback >(
                                 std::move( callback ) ) ).second )
        return false; // port already mapped

    // ask to map this port on all known devices
    for( auto& device : m_devices )
        enableMappingOnDevice( *device.second, false, request );

    return true;
}

void PortMapper::enableMappingOnDevice(
        Device& device, bool isCheck, std::pair< quint16, Protocol > request )
{
    const auto port = request.first;
    const auto protocol = request.second;
    auto callback = [ this, &device, port, protocol ]( const quint16& mappedPort )
    {
        QMutexLocker lock( &m_mutex );
        const auto it = m_mappings.find( std::make_pair( port, protocol ) );
        if( it != m_mappings.end() )
        {
            const std::shared_ptr< Callback > control = it->second;
            lock.unlock();
            control->call( MappingInfo( port, mappedPort, device.externalIp() ) );
        }

        if( --m_asyncInProgress == 0)
            m_asyncCondition.wakeOne();
    };

    const auto result = isCheck
            ? device.check( port, protocol, std::move( callback ) )
            : device.map( port, port, protocol, std::move( callback ) );

    if( result )
        ++m_asyncInProgress;
}

bool PortMapper::disableMapping( quint16 port, Protocol protocol, bool waitForFinish )
{
    std::shared_ptr< Callback > callback;
    {
        QMutexLocker lock( &m_mutex );
        const auto it = m_mappings.find( std::make_pair( port, protocol ) );
        if( it == m_mappings.end() )
            return false;

        std::swap( callback, it->second );
        m_mappings.erase( it );
        for( auto& device : m_devices )
        {
            const auto result = device.second->unmap( port, protocol, [ this ]()
            {
                QMutexLocker lock( &m_mutex );
                if( --m_asyncInProgress == 0)
                    m_asyncCondition.wakeOne();
            } );

            if( result )
                ++m_asyncInProgress;
        }
    }

    if( waitForFinish )
        callback->clear();

    return true;
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

        {
            QUrl url;
            url.setHost( devAddress.address.toString() );
            url.setPort( devAddress.port );
            url.setPath( service.controlUrl );

            std::unique_ptr< Device > device(
                new Device( m_upnpClient.get(), localAddress, url, m_description ) );

            QMutexLocker lk( &m_mutex );
            const auto itBool = m_devices.emplace( devInfo.serialNumber, std::move( device ) );
            if( !itBool.second )
                continue; // known device

            // ask to map all existing mappings on this device as well
            for( const auto& mapping : m_mappings )
                enableMappingOnDevice( *itBool.first->second, false, mapping.first );
        }
    }

    for( const auto& subDev : devInfo.deviceList )
        atLeastOneFound |= searchForMappers( localAddress, devAddress, subDev );

    return atLeastOneFound;
}

void PortMapper::onTimer( const quint64& /*timerID*/ )
{
    QMutexLocker lock( &m_mutex );
    for( const auto& mapping : m_mappings )
        for( auto& device : m_devices )
            enableMappingOnDevice( *device.second, true, mapping.first );

    if( m_timerId )
        m_timerId = TimerManager::instance()->addTimer( this, m_checkMappingsInterval );
}

} // namespace nx_upnp
