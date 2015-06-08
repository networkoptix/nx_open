#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>
#include <plugins/resource/upnp/upnp_port_mapper_internal.h>

#include <QtConcurrent>

UpnpAsyncClientMock::UpnpAsyncClientMock( const HostAddress& externalIp )
    : m_externalIp( externalIp )
    , m_disabledPort( 80 )
{
}

bool UpnpAsyncClientMock::externalIp(
        const QUrl& /*url*/,
        const std::function< void( const HostAddress& ) >& callback )
{
    QtConcurrent::run( [ this, callback ]{ callback( m_externalIp ); } );
    return true;
}

bool UpnpAsyncClientMock::addMapping(
        const QUrl& /*url*/, const HostAddress& internalIp,
        quint16 internalPort, quint16 externalPort, const QString& protocol,
        const std::function< void( bool ) >& callback )
{
    if( externalPort != m_disabledPort &&
            m_mappings.find( externalPort ) == m_mappings.end() )
    {
        m_mappings[ externalPort ] = SocketAddress( internalIp, internalPort );
        QtConcurrent::run( [ callback ]{ callback( true ); } );
        return true;
    }

    QtConcurrent::run( [ callback ]{ callback( false ); } );
    return true;
}

bool UpnpAsyncClientMock::deleteMapping(
        const QUrl& /*url*/, quint16 externalPort, const QString& protocol,
        const std::function< void( bool ) >& callback )
{
    const bool isErased = m_mappings.erase( externalPort );
    QtConcurrent::run( [ isErased, callback ]{ callback( isErased ); } );
    return true;
}

UpnpPortMapperMocked::UpnpPortMapperMocked( const HostAddress& internalIp,
                                            const HostAddress& externalIp )
    : UpnpPortMapper( QString() )
{
    m_upnpClient.reset( new UpnpAsyncClientMock( externalIp ) );

    UpnpDeviceInfo::Service service;
    service.serviceType = UpnpAsyncClient::WAN_IP;
    service.controlUrl = lit( "/someUrl" );

    UpnpDeviceInfo devInfo;
    devInfo.serialNumber = lit( "XXX" );
    devInfo.serviceList.push_back( service );

    searchForMappers( internalIp, SocketAddress(), devInfo );
}

UpnpAsyncClientMock& UpnpPortMapperMocked::clientMock()
{
    return *static_cast< UpnpAsyncClientMock* >( m_upnpClient.get() );
}
