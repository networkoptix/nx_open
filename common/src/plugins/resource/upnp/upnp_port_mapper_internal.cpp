#include "upnp_port_mapper_internal.h"

const quint16 RETRY_PORT_STEP = 10;
const quint16 RETRY_PORT_MAX  = 50;

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

UpnpPortMapper::MappingDevice::MappingDevice( UpnpAsyncClient* upnpClient,
                                              const HostAddress& internalIp,
                                              const QUrl& url )
    : m_upnpClient( upnpClient )
    , m_internalIp( internalIp )
    , m_url( url )
{
    m_upnpClient->externalIp( url, [this]( const HostAddress& newIp )
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

void UpnpPortMapper::MappingDevice::map( quint16 port, quint16 desiredPort,
                                         const std::function< void( quint16 ) >& callback )
{
    // FIXME: what if addMapping returns false?
    m_upnpClient->addMapping( m_url, m_internalIp, port, desiredPort, lit( "TCP" ),
                              // TODO: move protocol to paramiters
                              [this, callback, port, desiredPort]( bool success )
    {
        if( !success )
        {
            const auto newPort = desiredPort + qrand() % RETRY_PORT_STEP;
            if( newPort - port > RETRY_PORT_MAX )
            {
                callback( 0 ); // mapping is not possible
                return;
            }

            // retry another port
            map( port, newPort, callback );
            return;
        }

        {
            QMutexLocker lk( &m_mutex );
            if( m_externalIp == HostAddress::anyHost )
            {
                // will be notified as soon as we know external IP
                m_successQueue[ port ] = std::bind( callback, desiredPort );
                return;
            }
        }

        callback( desiredPort );
    });
}

void UpnpPortMapper::MappingDevice::unmap( quint16 port,
                                           const std::function< void() >& callback )
{
    // FIXME: what if deleteMapping returns false?
    m_upnpClient->deleteMapping( m_url, port, lit( "TCP" ), [ callback ]( bool )
    {
        // FIXME: what if call was not successful, shell we retry? how many times? :)
        callback();
    } );
}
