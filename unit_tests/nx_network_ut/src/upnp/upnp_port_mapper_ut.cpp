#include <gtest/gtest.h>

#include <nx/network/upnp/upnp_device_searcher.h>
#include <nx/utils/test_support/sync_queue.h>

#include "upnp_port_mapper_mocked.h"

namespace nx_upnp {
namespace test {

class UpnpPortMapper: public ::testing::Test
{
public:
    UpnpPortMapper() : deviceSearcher(deviceSearcherSettings) {}
    nx::utils::TimerManager timerManager;
    DeviceSearcherDefaultSettings deviceSearcherSettings;
    DeviceSearcher deviceSearcher;

    static std::pair< quint16, PortMapper::Protocol > tcpPort( quint16 port )
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
};

TEST_F( UpnpPortMapper, NormalUsage )
{
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001 and 80
    nx::utils::TestSyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( info ); } ) );

    const auto map7001 = popAddress( &queue7001 );
    EXPECT_EQ( map7001.address.toString(), lit( "12.34.56.78" ) );
    EXPECT_EQ( clientMock.mappings().size(), 1U );

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
    EXPECT_EQ( clientMock.mappingsCount(), 2U );

    // Unmap 7001 and 80
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1U );

    const auto addr80 = clientMock.mappings()[ tcpPort( map80.port ) ];
    EXPECT_EQ( addr80.first.toString(), lit( "192.168.0.10:80" ) );

    EXPECT_TRUE( portMapper.disableMapping( 80, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0U );
}

TEST_F( UpnpPortMapper, ReuseExisting )
{
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // simulate mapping 6666 -> 192.168.0.10:7001
    EXPECT_TRUE( clientMock.mkMapping( std::make_pair(
        std::make_pair( 6666, PortMapper::Protocol::TCP ),
        std::make_pair( SocketAddress( lit( "192.168.0.10" ), 7001 ), QString() ) ) ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1U );

    nx::utils::TestSyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( std::move( info ) ); } ) );

    // existed mapping should be in use
    EXPECT_EQ( popAddress( &queue7001 ).toString(), lit( "12.34.56.78:6666" ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1U );

    // existed mapping should be removed when if need
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0U );
}

TEST_F( UpnpPortMapper, CheckMappings )
{
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ), 500 );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001
    nx::utils::TestSyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( std::move(info) ); } ) );

    EXPECT_EQ( popAddress( &queue7001 ).address.toString(), lit( "12.34.56.78" ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1U );
    clientMock.mappings().clear();

    QThread::sleep( 1 ); // wait for mapping to be restored
    EXPECT_EQ( clientMock.mappings().size(), 1U );

    QThread::sleep( 1 ); // wait a little longer to be sure nothing got broken
    EXPECT_EQ( clientMock.mappings().size(), 1U );

    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0U );

    QThread::sleep( 1 ); // this time mapping wount get restored
    EXPECT_EQ( clientMock.mappings().size(), 0U );
}

TEST_F( UpnpPortMapper, DISABLED_RealRouter )
{
    PortMapper portMapper( true, 10000 );

    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress ) {} ) );

    QThread::sleep( 60 * 60 ); // vary long time ;)
}

} // namespace test
} // namespace nx_upnp
