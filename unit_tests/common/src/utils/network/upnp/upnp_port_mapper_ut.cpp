#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>
#include <utils/network/upnp/upnp_port_mapper_internal.h>

#include <gtest.h>
#include <queue>

namespace nx_upnp {
namespace test {

std::pair< quint16, PortMapper::Protocol > tcpPort( quint16 port )
{
    return std::make_pair( port, PortMapper::Protocol::TCP );
}

template<typename T>
class SyncQueue
{
public:
    void push( const T& item )
    {
        QMutexLocker lock( &m_mutex );
        m_queue.push( item );
        if( m_queue.size() == 1)
            m_condition.wakeOne();
    }

    T pop()
    {
        QMutexLocker locak( &m_mutex );
        while( m_queue.empty() )
            m_condition.wait( &m_mutex );

        T val = std::move( m_queue.front() );
        m_queue.pop();
        return val;
    }

private:
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::queue< T > m_queue;
};

TEST( UpnpPortMapper, Normal )
{
    TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    UpnpPortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    UpnpAsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001 and 80
    SyncQueue< PortMapper::MappingInfo > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( const PortMapper::MappingInfo& info )
                 { queue7001.push( info ); } ) );

    const auto map7001 = queue7001.pop();
    EXPECT_EQ( map7001.internalPort, 7001 );
    EXPECT_EQ( map7001.externalPort, 7001 );
    EXPECT_EQ( map7001.externalIp, "12.34.56.78" );
    EXPECT_EQ( clientMock.m_mappings.size(), 1 );

    const auto addr7001 = clientMock.m_mappings[ tcpPort( 7001 ) ].first;
    EXPECT_EQ( addr7001.toString().toUtf8(), QByteArray( "192.168.0.10:7001" ) );

    SyncQueue< PortMapper::MappingInfo > queue80;
    EXPECT_TRUE( portMapper.enableMapping( 80, PortMapper::Protocol::TCP,
                 [&]( const PortMapper::MappingInfo& info )
                 { queue80.push( info ); } ) );

    const auto map80 = queue80.pop();
    EXPECT_EQ( map80.internalPort, 80 );
    EXPECT_EQ( map80.externalIp, "12.34.56.78" );
    ASSERT_GT( map80.externalPort, 80 );
    EXPECT_EQ( clientMock.m_mappings.size(), 2 );

    // Unmap 7001 and 80
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.m_mappings.size(), 1 );

    const auto addr80 = clientMock.m_mappings[ tcpPort( map80.externalPort ) ].first;
    EXPECT_EQ( addr80.toString().toUtf8(), QByteArray( "192.168.0.10:80" ) );

    EXPECT_TRUE( portMapper.disableMapping( 80, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.m_mappings.size(), 0 );
}

TEST( UpnpPortMapper, ReuseExisting )
{
    TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    UpnpPortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    UpnpAsyncClientMock& clientMock = portMapper.clientMock();

    // simulate mapping 6666 -> 192.168.0.10:7001
    clientMock.m_mappings.insert( std::make_pair(
        std::make_pair( 6666, PortMapper::Protocol::TCP ),
        std::make_pair( SocketAddress( lit( "192.168.0.10" ), 7001 ), QString() ) ) );
    EXPECT_EQ( clientMock.m_mappings.size(), 1 );

    SyncQueue< PortMapper::MappingInfo > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( const PortMapper::MappingInfo& info )
                 { queue7001.push( info ); } ) );

    const auto map7001 = queue7001.pop(); // existed mapping should be in use
    EXPECT_EQ( map7001.internalPort, 7001 );
    EXPECT_EQ( map7001.externalPort, 6666 );
    EXPECT_EQ( map7001.externalIp, "12.34.56.78" );
    EXPECT_EQ( clientMock.m_mappings.size(), 1 );

    // existed mapping should be removed when if need
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.m_mappings.size(), 0 );
}

} // namespace test
} // namespace nx_upnp
