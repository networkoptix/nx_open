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
        quint16 internalPort, quint16 externalPort,
        Protocol protocol, const QString& description,
        const std::function< void( bool ) >& callback )
{
    if( externalPort != m_disabledPort &&
            m_mappings.find( std::make_pair( externalPort, protocol ) )
                != m_mappings.end() )
    {
        m_mappings[ std::make_pair( externalPort, protocol ) ]
            = std::make_pair( SocketAddress( internalIp, internalPort ), description );

        QtConcurrent::run( [ callback ]{ callback( true ); } );
        return true;
    }

    QtConcurrent::run( [ callback ]{ callback( false ); } );
    return true;
}

bool UpnpAsyncClientMock::deleteMapping(
        const QUrl& /*url*/, quint16 externalPort, Protocol protocol,
        const std::function< void( bool ) >& callback )
{
    const bool isErased = m_mappings.erase( std::make_pair( externalPort, protocol ) );
    QtConcurrent::run( [ isErased, callback ]{ callback( isErased ); } );
    return true;
}

bool UpnpAsyncClientMock::getMapping(
        const QUrl& /*url*/, quint32 index,
        const std::function< void( const MappingInfo& ) >& callback )
{
    if( m_mappings.size() >= index )
    {
        QtConcurrent::run( [ callback ]{ callback( MappingInfo() ); } );
        return true;
    }

    const auto mapping = *std::next( m_mappings.begin(), index );
    QtConcurrent::run( [ callback, mapping ]{
        callback( MappingInfo(
            mapping.second.first.address,
            mapping.second.first.port,
            mapping.first.first,
            mapping.first.second,
            mapping.second.second ) );
    } );

    return true;
}

bool UpnpAsyncClientMock::getMapping(
        const QUrl& /*url*/, quint16 externalPort, Protocol protocol,
        const std::function< void( const MappingInfo& ) >& callback )
{
    const auto it = m_mappings.find( std::make_pair( externalPort, protocol ) );
    if( it == m_mappings.end() )
    {
        QtConcurrent::run( [ callback ]{ callback( MappingInfo() ); } );
        return true;
    }

    const auto mapping = *it;
    QtConcurrent::run( [ callback, mapping ]{
        callback( MappingInfo(
            mapping.second.first.address,
            mapping.second.first.port,
            mapping.first.first,
            mapping.first.second,
            mapping.second.second ) );
    } );

    return true;
}

UpnpPortMapperMocked::UpnpPortMapperMocked( const HostAddress& internalIp,
                                            const HostAddress& externalIp )
    : UpnpPortMapper( lit( "UpnpPortMapperMocked" ), QString() )
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
