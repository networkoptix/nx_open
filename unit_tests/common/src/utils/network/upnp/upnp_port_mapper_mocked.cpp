#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>
#include <utils/network/upnp/upnp_port_mapper_internal.h>

#include <QtConcurrent>

namespace nx_upnp {
namespace test {

AsyncClientMock::AsyncClientMock( const HostAddress& externalIp )
    : m_externalIp( externalIp )
    , m_disabledPort( 80 )
{
}

bool AsyncClientMock::externalIp(
        const QUrl& /*url*/,
        const std::function< void( const HostAddress& ) >& callback )
{
    QtConcurrent::run( [ this, callback ]{ callback( m_externalIp ); } );
    return true;
}

bool AsyncClientMock::addMapping(
        const QUrl& /*url*/, const HostAddress& internalIp,
        quint16 internalPort, quint16 externalPort,
        Protocol protocol, const QString& description,
        const std::function< void( bool ) >& callback )
{
    QMutexLocker lock( &m_mutex );
    if( externalPort == m_disabledPort ||
            m_mappings.find( std::make_pair( externalPort, protocol ) )
                != m_mappings.end() )
    {
        QtConcurrent::run( [ callback ]{ callback( false ); } );
        return true;
    }

    m_mappings[ std::make_pair( externalPort, protocol ) ]
        = std::make_pair( SocketAddress( internalIp, internalPort ), description );

    QtConcurrent::run( [ callback ]{ callback( true ); } );
    return true;

}

bool AsyncClientMock::deleteMapping(
        const QUrl& /*url*/, quint16 externalPort, Protocol protocol,
        const std::function< void( bool ) >& callback )
{
    QMutexLocker lock( &m_mutex );
    const bool isErased = m_mappings.erase( std::make_pair( externalPort, protocol ) );
    QtConcurrent::run( [ isErased, callback ]{ callback( isErased ); } );
    return true;
}

bool AsyncClientMock::getMapping(
        const QUrl& /*url*/, quint32 index,
        const std::function< void( const MappingInfo& ) >& callback )
{
    QMutexLocker lock( &m_mutex );
    if( m_mappings.size() <= index )
    {
        QtConcurrent::run( [ callback ]{ callback( MappingInfo() ); } );
        return true;
    }

    const auto mapping = *std::next( m_mappings.begin(), index );
    QtConcurrent::run( [ callback, mapping ]
    {
        callback( MappingInfo(
            mapping.second.first.address,
            mapping.second.first.port,
            mapping.first.first,
            mapping.first.second,
            mapping.second.second ) );
    } );

    return true;
}

bool AsyncClientMock::getMapping(
        const QUrl& /*url*/, quint16 externalPort, Protocol protocol,
        const std::function< void( const MappingInfo& ) >& callback )
{
    QMutexLocker lock( &m_mutex );
    const auto it = m_mappings.find( std::make_pair( externalPort, protocol ) );
    if( it == m_mappings.end() )
    {
        QtConcurrent::run( [ callback ]{ callback( MappingInfo() ); } );
        return true;
    }

    const auto mapping = *it;
    QtConcurrent::run( [ callback, mapping ]
    {
        callback( MappingInfo(
            mapping.second.first.address,
            mapping.second.first.port,
            mapping.first.first,
            mapping.first.second,
            mapping.second.second ) );
    } );

    return true;
}

AsyncClientMock::Mappings AsyncClientMock::mappings() const
{
    QMutexLocker lock( &m_mutex );
    return m_mappings;
}

size_t AsyncClientMock::mappingsCount() const
{
    QMutexLocker lock( &m_mutex );
    return m_mappings.size();
}

bool AsyncClientMock::mkMapping( const Mappings::value_type& value )
{
    QMutexLocker lock( &m_mutex );
    return m_mappings.insert( value ).second;
}


bool AsyncClientMock::rmMapping( quint16 port, Protocol protocol )
{
    QMutexLocker lock( &m_mutex );
    return m_mappings.erase( std::make_pair( port, protocol ) );
}

PortMapperMocked::PortMapperMocked( const HostAddress& internalIp,
                                    const HostAddress& externalIp,
                                    quint64 checkMappingsInterval )
    : PortMapper( checkMappingsInterval, lit( "UpnpPortMapperMocked" ), QString() )
{
    m_upnpClient.reset( new AsyncClientMock( externalIp ) );

    DeviceInfo::Service service;
    service.serviceType = AsyncClient::WAN_IP;
    service.controlUrl = lit( "/someUrl" );

    DeviceInfo devInfo;
    devInfo.serialNumber = lit( "XXX" );
    devInfo.serviceList.push_back( service );

    searchForMappers( internalIp, SocketAddress(), devInfo );
}

AsyncClientMock& PortMapperMocked::clientMock()
{
    return *static_cast< AsyncClientMock* >( m_upnpClient.get() );
}

} // namespace test
} // namespace nx_upnp
