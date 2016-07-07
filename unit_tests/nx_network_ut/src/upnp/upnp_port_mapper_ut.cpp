#include <gtest/gtest.h>

#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>
#include <api/global_settings.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx_upnp {
namespace test {

std::pair< quint16, PortMapper::Protocol > tcpPort( quint16 port )
{
    return std::make_pair( port, PortMapper::Protocol::TCP );
}

static SocketAddress popAddress( nx::utils::TestSyncQueue< SocketAddress >* queue7001 )
{
    auto address = queue7001->pop();
    if( address.address == HostAddress() ) // external IP is not resolved yet
        address = queue7001->pop(); // wait for the next notification

    return address;
}

TEST( UpnpPortMapper, NormalUsage )
{
    nx::utils::TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001 and 80
    nx::utils::TestSyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( info ); } ) );

    const auto map7001 = popAddress( &queue7001 );
    EXPECT_EQ( map7001.address.toString(), lit( "12.34.56.78" ) );
    EXPECT_EQ( clientMock.mappings().size(), 1 );

    const auto addr7001 = *clientMock.mappings().begin();
    EXPECT_EQ( addr7001.first, tcpPort( map7001.port ) );
    EXPECT_EQ( addr7001.second.first.toString(), lit( "192.168.0.10:7001" ) );

    nx::utils::TestSyncQueue< SocketAddress > queue80;
    EXPECT_TRUE( portMapper.enableMapping( 80, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue80.push( info ); } ) );

    const auto map80 = queue80.pop();
    EXPECT_EQ( map80.address.toString(), lit( "12.34.56.78" ) );
    EXPECT_GT( map80.port, 80 );
    EXPECT_EQ( clientMock.mappingsCount(), 2 );

    // Unmap 7001 and 80
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );

    const auto addr80 = clientMock.mappings()[ tcpPort( map80.port ) ];
    EXPECT_EQ( addr80.first.toString(), lit( "192.168.0.10:80" ) );

    EXPECT_TRUE( portMapper.disableMapping( 80, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0 );
}

TEST( UpnpPortMapper, ReuseExisting )
{
    nx::utils::TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // simulate mapping 6666 -> 192.168.0.10:7001
    EXPECT_TRUE( clientMock.mkMapping( std::make_pair(
        std::make_pair( 6666, PortMapper::Protocol::TCP ),
        std::make_pair( SocketAddress( lit( "192.168.0.10" ), 7001 ), QString() ) ) ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );

    nx::utils::TestSyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( std::move( info ) ); } ) );

    // existed mapping should be in use
    EXPECT_EQ( popAddress( &queue7001 ).toString(), lit( "12.34.56.78:6666" ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );

    // existed mapping should be removed when if need
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0 );
}

TEST( UpnpPortMapper, CheckMappings )
{
    nx::utils::TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ), 500 );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001
    nx::utils::TestSyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( std::move(info) ); } ) );

    EXPECT_EQ( popAddress( &queue7001 ).address.toString(), lit( "12.34.56.78" ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );
    clientMock.mappings().clear();

    QThread::sleep( 1 ); // wait for mapping to be restored
    EXPECT_EQ( clientMock.mappings().size(), 1 );

    QThread::sleep( 1 ); // wait a little longer to be sure nothing got broken
    EXPECT_EQ( clientMock.mappings().size(), 1 );

    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0 );

    QThread::sleep( 1 ); // this time mapping wount get restored
    EXPECT_EQ( clientMock.mappings().size(), 0 );
}

TEST( UpnpPortMapper, DISABLED_RealRouter )
{
    nx::utils::TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapper portMapper( 10000 );

    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress ) {} ) );

    QThread::sleep( 60 * 60 ); // vary long time ;)
}

} // namespace test
} // namespace nx_upnp
