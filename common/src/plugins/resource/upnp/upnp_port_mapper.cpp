#include "upnp_port_mapper.h"

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
    device.map( port, [ this, &device, port ]( const quint16& mappedPort )
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_mappings.find( port );
        if (it != m_mappings.end())
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

        // TODO: add request to all devices to unmap
        std::swap( callback, it->second );
        m_mappings.erase( it );
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

            // Ask to map all existing mappings on this device as well
            for( const auto& mapping : m_mappings )
                enableMappingOnDevice( *itBool.first->second, mapping.first );
        }

        for( const auto& subDev : devInfo.deviceList )
            atLeastOneFound |= searchForMappers( localAddress, devAddress, subDev );
    }

    return atLeastOneFound;
}

UpnpPortMapper::CallbackControl::CallbackControl( const MappingCallback& callback )
    : m_callback( callback )
{
}

void UpnpPortMapper::CallbackControl::call( const MappingInfo& info )
{
    QMutexLocker lk( &m_mutex );
    if( m_callback )
        m_callback( info );
}

void UpnpPortMapper::CallbackControl::clear()
{
    QMutexLocker lk( &m_mutex );
    m_callback = nullptr;
}

UpnpPortMapper::MappingDevice::MappingDevice( UpnpAsyncClient& upnpClient,
                                              const HostAddress& internalIp,
                                              const QUrl& url )
    : m_upnpClient( upnpClient )
    , m_internalIp( internalIp )
    , m_url( url )
{
    m_upnpClient.externalIp( url, [this]( const HostAddress& newIp )
    {
        std::map< quint16, std::function< void() > > successQueue;
        {
            QMutexLocker lk( &m_mutex );
            m_externalIp = newIp;
            std::swap( m_successQueue, successQueue );
        }

        // notify all callbacks in queue (they were not notified bc externalIp was unknown
        for( const auto it : successQueue )
            it.second();
    } );
}

HostAddress UpnpPortMapper::MappingDevice::externalIp() const
{
    QMutexLocker lk( &m_mutex );
    return m_externalIp;
}

void UpnpPortMapper::MappingDevice::map( quint16 port,
                                         const std::function< void( quint16 ) >& callback )
{
    m_upnpClient.addMapping( m_url, m_internalIp, port, port, lit( "TCP" ),
                              // TODO: move protocol to paramiters
                              [this, callback, port]( bool success )
    {
        if( !success )
            return;

        {
            QMutexLocker lk( &m_mutex );
            if( m_externalIp == HostAddress::anyHost )
            {
                // will be notified as soon as we know external IP
                m_successQueue[ port ] = std::bind( callback, port );
                return;
            }
        }

        callback( port );
    });
}

void UpnpPortMapper::MappingDevice::unmap( quint16 port )
{
    // TODO: implement
    static_cast<void>( port );
}
