#include <utils/network/upnp/upnp_async_client.h>

#include <common/common_globals.h>

#include <gtest.h>
#include <future>

namespace nx_upnp {
namespace test {

static const QUrl URL( lit("http://192.168.1.1:57166/ctl/IPConn") );
static const auto TCP = AsyncClient::Protocol::TCP;
static const QString IP = lit( "192.168.1.170" );
static const QString DESC = lit( "test" );

// TODO: implement over test HTTP server
TEST( UpnpAsyncClient, DISABLED_GetIp )
{
    AsyncClient client;

    {
        std::promise< HostAddress > prom;
        EXPECT_TRUE( client.externalIp( URL,
                     [&]( const HostAddress& v ) { prom.set_value( v ); } ) );
        EXPECT_EQ( prom.get_future().get().toString().toUtf8(), QByteArray( "10.0.2.130" ) );
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
         EXPECT_TRUE( client.addMapping( URL, IP, 80, 80, TCP, DESC,
                      [&]( bool v ) { prom.set_value( v ); } ) );

         EXPECT_FALSE( prom.get_future().get() ); // wrong port
    }
    {
        std::promise< bool > prom;
        EXPECT_TRUE( client.addMapping( URL, IP, 80, 8877, TCP, DESC,
                     [&]( bool v ) { prom.set_value( v ); } ) );

        ASSERT_TRUE( prom.get_future().get() );
    }
    {
        std::promise< SocketAddress > prom;
        EXPECT_TRUE( client.getMapping( URL, 8877, TCP,
                     [&]( const AsyncClient::MappingInfo& m )
                     { prom.set_value( SocketAddress( m.internalIp,
                                                      m.internalPort ) ); } ) );

        EXPECT_EQ( prom.get_future().get().toString().toUtf8(),
                   SocketAddress( IP, 80 ).toString().toUtf8() );
    }
    {
        std::promise< bool > prom;
        EXPECT_TRUE( client.deleteMapping( URL, 8877, TCP,
                     [&]( bool v ) { prom.set_value( v ); } ) );

        EXPECT_TRUE( prom.get_future().get() ); // no such mapping
    }
}

} // namespace test
} // namespace nx_upnp
