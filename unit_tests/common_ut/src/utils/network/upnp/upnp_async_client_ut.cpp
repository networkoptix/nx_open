#include <utils/network/upnp/upnp_async_client.h>

#include <common/common_globals.h>

#include <gtest.h>
#include <future>

namespace nx_upnp {
namespace test {

static const QUrl URL( lit("http://192.168.20.1:52869/ctl/IPConn") );
static const auto TCP = AsyncClient::Protocol::TCP;
static const QString INTERNAL_IP = lit( "192.168.20.77" );
static const QString EXTERNAL_IP = lit( "10.0.2.161" );
static const QString DESC = lit( "test" );

// TODO: implement over test HTTP server
TEST( UpnpAsyncClient, DISABLED_GetIp )
{
    AsyncClient client;
    {
        std::promise< HostAddress > prom;
        EXPECT_TRUE( client.externalIp( URL,
                     [&]( const HostAddress& v ) { prom.set_value( v ); } ) );
        EXPECT_EQ( prom.get_future().get().toString(), EXTERNAL_IP );
    }
    {
        std::promise< AsyncClient::MappingInfo > prom;
        EXPECT_TRUE( client.getMapping( URL, 8877, TCP,
                     [&]( const AsyncClient::MappingInfo& mapping )
                     { prom.set_value( mapping ); } ) );

        // no such mapping
        EXPECT_FALSE( prom.get_future().get().isValid() );
    }
    {
        std::promise< bool > prom;
        EXPECT_TRUE( client.deleteMapping( URL, 8877, TCP,
                     [&]( bool v ) { prom.set_value( v ); } ) );
        EXPECT_FALSE( prom.get_future().get() ); // no such mapping
    }
    {
        std::promise< AsyncClient::MappingList > prom;
        EXPECT_TRUE( client.getAllMappings( URL,
                     [&]( const AsyncClient::MappingList& v ) { prom.set_value( v ); } ) );
        EXPECT_EQ( prom.get_future().get().size(), 2 ); // mappings from Skype ;)
    }
}

// TODO: implement over test HTTP server
TEST( UpnpAsyncClient, DISABLED_Mapping )
{
    AsyncClient client;
    {
         std::promise< bool > prom;
         EXPECT_TRUE( client.addMapping( URL, INTERNAL_IP, 80, 80, TCP, DESC, 0,
                      [&]( bool v ) { prom.set_value( v ); } ) );

         EXPECT_FALSE( prom.get_future().get() ); // wrong port
    }
    {
        std::promise< bool > prom;
        EXPECT_TRUE( client.addMapping( URL, INTERNAL_IP, 80, 8877, TCP, DESC, 0,
                     [&]( bool v ) { prom.set_value( v ); } ) );

        ASSERT_TRUE( prom.get_future().get() );
    }
    {
        std::promise< SocketAddress > prom;
        EXPECT_TRUE( client.getMapping( URL, 8877, TCP,
                     [&]( const AsyncClient::MappingInfo& m )
                     { prom.set_value( SocketAddress( m.internalIp,
                                                      m.internalPort ) ); } ) );

        EXPECT_EQ( prom.get_future().get().toString(),
                   SocketAddress( INTERNAL_IP, 80 ).toString() );
    }
    {
        std::promise< bool > prom;
        EXPECT_TRUE( client.deleteMapping( URL, 8877, TCP,
                     [&]( bool v ) { prom.set_value( v ); } ) );

        EXPECT_TRUE( prom.get_future().get() );
    }
}

} // namespace test
} // namespace nx_upnp
