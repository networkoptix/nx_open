#include "upnp_port_mapper.h"

#include "upnp_port_mapper_internal.h"

static const quint64 CHECK_MAPPINGS_INTERVAL = 1 * 60; // a minute

UpnpPortMapper::UpnpPortMapper( const QString& description, const QString& device )
    : UPNPSearchAutoHandler( device )
    , m_upnpClient( new UpnpAsyncClient() )
    , m_asyncInProgress( 0 )
    , m_description( description )
{
    m_timerId = TimerManager::instance()->addTimer( this, CHECK_MAPPINGS_INTERVAL );
}

UpnpPortMapper::~UpnpPortMapper()
{
    quint64 timerId;
    {
        QMutexLocker lock( &m_mutex );
        timerId = m_timerId;
    }

    TimerManager::instance()->joinAndDeleteTimer( timerId );

    QMutexLocker lock( &m_mutex );
    while( m_asyncInProgress )
        m_asyncCondition.wait( &m_mutex );
}

UpnpPortMapper::MappingInfo::MappingInfo( quint16 inPort, quint16 exPort,
                                          const HostAddress& exIp, Protocol prot )
    : internalPort( inPort ), externalPort( exPort )
    , externalIp( exIp ), protocol( prot )
{
}

bool UpnpPortMapper::enableMapping(
        quint16 port, Protocol protocol,
        const std::function< void( const MappingInfo& ) >& callback )
{
    const auto request = std::make_pair( port, protocol );

    QMutexLocker lock( &m_mutex );
    if( !m_mappings.emplace( request, std::make_shared< CallbackControl >( callback ) ).second )
        return false; // port already mapped

    // ask to map this port on all known devices
    for( auto& device : m_devices )
        enableMappingOnDevice( *device.second, request );

    return true;
}

void UpnpPortMapper::enableMappingOnDevice(
        MappingDevice& device, std::pair< quint16, Protocol > request )
{
    const auto port = request.first;
    const auto protocol = request.second;
    const auto result = device.map( port, port, protocol,
            [ this, &device, port, protocol ]( const quint16& mappedPort )
    {   
        QMutexLocker lock( &m_mutex );
        const auto it = m_mappings.find( std::make_pair( port, protocol ) );
        if( it != m_mappings.end() )
        {
            const std::shared_ptr< CallbackControl > control = it->second;
            lock.unlock();
            control->call( MappingInfo( port, mappedPort, device.externalIp() ) );
        }

        if( --m_asyncInProgress == 0)
            m_asyncCondition.wakeOne();
    } );

    if( result )
        ++m_asyncInProgress;
}

bool UpnpPortMapper::disableMapping( quint16 port, Protocol protocol, bool waitForFinish )
{
    std::shared_ptr< CallbackControl > callback;
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

bool UpnpPortMapper::processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const UpnpDeviceInfo& devInfo, const QByteArray& xmlDevInfo )
{
    static_cast< void >( xmlDevInfo );
    return searchForMappers( HostAddress( localAddress.toString() ), devAddress, devInfo );
}

bool UpnpPortMapper::searchForMappers( const HostAddress& localAddress,
                                       const SocketAddress& devAddress,
                                       const UpnpDeviceInfo& devInfo )
{
    bool atLeastOneFound = false;
    for( const auto& service : devInfo.serviceList)
    {
        if( service.serviceType != UpnpAsyncClient::WAN_IP )
            continue; // uninteresting

        {
            QUrl url( devAddress.toString() );
            url.setPath( service.controlUrl );

            std::unique_ptr< MappingDevice > device(
                new MappingDevice( m_upnpClient.get(), localAddress, url, m_description ) );

            QMutexLocker lk( &m_mutex );
            const auto itBool = m_devices.emplace( devInfo.serialNumber, std::move( device ) );
            if( !itBool.second )
                continue; // known device

            // ask to map all existing mappings on this device as well
            for( const auto& mapping : m_mappings )
                enableMappingOnDevice( *itBool.first->second, mapping.first );
        }

        for( const auto& subDev : devInfo.deviceList )
            atLeastOneFound |= searchForMappers( localAddress, devAddress, subDev );
    }

    return atLeastOneFound;
}

void UpnpPortMapper::onTimer( const quint64& /*timerID*/ )
{
    QMutexLocker lock( &m_mutex );
    for( const auto& mapping : m_mappings )
        for( auto& device : m_devices )
            enableMappingOnDevice( *device.second, mapping.first );

    m_timerId = TimerManager::instance()->addTimer( this, CHECK_MAPPINGS_INTERVAL );
}
