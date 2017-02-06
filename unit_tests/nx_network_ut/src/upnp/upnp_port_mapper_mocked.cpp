
#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>

namespace nx_upnp {
namespace test {

AsyncClientMock::AsyncClientMock( const HostAddress& externalIp )
    : m_externalIp( externalIp )
    , m_disabledPort( 80 )
{
    m_thread = nx::utils::thread(
        [this]()
        {
            while (const auto task = m_tasks.pop())
                task();
        });
}

AsyncClientMock::~AsyncClientMock()
{
    m_tasks.push(nullptr);
    m_thread.join();
}

void AsyncClientMock::externalIp(
        const QUrl& /*url*/,
        std::function< void( const HostAddress& ) > callback )
{
    m_tasks.push( [ this, callback ]{ callback( m_externalIp ); } );
}

void AsyncClientMock::addMapping(
    const QUrl& /*url*/, const HostAddress& internalIp,
    quint16 internalPort, quint16 externalPort,
    Protocol protocol, const QString& description, quint64 /*duration*/,
    std::function< void( bool ) > callback )
{
    QnMutexLocker lock( &m_mutex );
    if( externalPort == m_disabledPort ||
            m_mappings.find( std::make_pair( externalPort, protocol ) )
                != m_mappings.end() )
    {
        return m_tasks.push( [ callback ]{ callback( false ); } );
    }

    m_mappings[ std::make_pair( externalPort, protocol ) ]
        = std::make_pair( SocketAddress( internalIp, internalPort ), description );

    m_tasks.push( [ callback ]{ callback( true ); } );
}

void AsyncClientMock::deleteMapping(
        const QUrl& /*url*/, quint16 externalPort, Protocol protocol,
        std::function< void( bool ) > callback )
{
    QnMutexLocker lock( &m_mutex );
    const bool isErased = m_mappings.erase( std::make_pair( externalPort, protocol ) );
    m_tasks.push( [ isErased, callback ]{ callback( isErased ); } );
}

void AsyncClientMock::getMapping(
        const QUrl& /*url*/, quint32 index,
        std::function< void( MappingInfo ) > callback )
{
    QnMutexLocker lock( &m_mutex );
    if( m_mappings.size() <= index )
        return m_tasks.push( [ callback ]{ callback( MappingInfo() ); } );

    const auto mapping = *std::next( m_mappings.begin(), index );
    m_tasks.push(
        [ callback, mapping ]
        {
            callback( MappingInfo(
                mapping.second.first.address,
                mapping.second.first.port,
                mapping.first.first,
                mapping.first.second,
                mapping.second.second ) );
        } );
}

void AsyncClientMock::getMapping(
        const QUrl& /*url*/, quint16 externalPort, Protocol protocol,
        std::function< void( MappingInfo ) > callback )
{
    QnMutexLocker lock( &m_mutex );
    const auto it = m_mappings.find( std::make_pair( externalPort, protocol ) );
    if( it == m_mappings.end() )
        return m_tasks.push( [ callback ]{ callback( MappingInfo() ); } );

    const auto mapping = *it;
    m_tasks.push(
        [ callback, mapping ]
        {
            callback( MappingInfo(
                mapping.second.first.address,
                mapping.second.first.port,
                mapping.first.first,
                mapping.first.second,
                mapping.second.second ) );
        } );
}

AsyncClientMock::Mappings AsyncClientMock::mappings() const
{
    QnMutexLocker lock( &m_mutex );
    return m_mappings;
}

size_t AsyncClientMock::mappingsCount() const
{
    QnMutexLocker lock( &m_mutex );
    return m_mappings.size();
}

bool AsyncClientMock::mkMapping( const Mappings::value_type& value )
{
    QnMutexLocker lock( &m_mutex );
    return m_mappings.insert( value ).second;
}


bool AsyncClientMock::rmMapping( quint16 port, Protocol protocol )
{
    QnMutexLocker lock( &m_mutex );
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
