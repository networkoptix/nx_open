#include "upnp_port_mapper.h"

#include "upnp_port_mapper_internal.h"

UpnpPortMapper::UpnpPortMapper()
    : UPNPSearchAutoHandler( UpnpAsyncClient::INTERNAL_GATEWAY )
{
}

bool UpnpPortMapper::enableMapping( quint16 port, const MappingCallback& callback )
{
    QMutexLocker lock(&m_mutex);
    if( !m_mappings.emplace( port, std::make_shared< CallbackControl >( callback ) ).second )
        return false; // port already mapped

    // ask to map this port on all known devices
    for( auto& device : m_devices )
        enableMappingOnDevice( *device.second, port );

    return true;
}

void UpnpPortMapper::enableMappingOnDevice( MappingDevice& device, quint16 port )
{
    device.map( port, port, [ this, &device, port ]( const quint16& mappedPort )
    {   
        QMutexLocker lock(&m_mutex);
        const auto it = m_mappings.find( port );
        if( it != m_mappings.end() )
        {
            const std::shared_ptr< CallbackControl > control = it->second;
            lock.unlock();
            control->call( MappingInfo( port, mappedPort, device.externalIp() ) );
        }
    } );
}

bool UpnpPortMapper::disableMapping( quint16 port, bool waitForFinish )
{
    std::shared_ptr< CallbackControl > callback;
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_mappings.find( port );
        if( it != m_mappings.end() )
            return false;

        std::swap( callback, it->second );
        m_mappings.erase( it );
        for( auto& device : m_devices )
            device.second->unmap( port );
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

            std::unique_ptr< MappingDevice > device( new MappingDevice(
                m_upnpClient, localAddress, url ) );

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
