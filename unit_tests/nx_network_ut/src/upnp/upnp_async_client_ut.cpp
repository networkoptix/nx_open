#include <nx/network/upnp/upnp_async_client.h>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>


namespace nx_upnp {
namespace test {

static const nx::utils::Url URL( lit("http://192.168.20.1:52869/ctl/IPConn") );
static const auto TCP = AsyncClient::Protocol::TCP;
static const QString INTERNAL_IP = lit( "192.168.20.77" );
static const QString EXTERNAL_IP = lit( "10.0.2.161" );
static const QString DESC = lit( "test" );

TEST( UpnpAsyncClient, DISABLED_Cancel )
{
    for (size_t testNumber = 0; testNumber <= 5; ++testNumber)
    {
        AsyncClient client;
        client.externalIp( URL, [&]( const HostAddress& ) { } );
        if (testNumber)
        {
            std::chrono::milliseconds delay(testNumber * testNumber * 200);
            std::this_thread::sleep_for(delay);
        }
    }
}

// TODO: implement over test HTTP server
TEST( UpnpAsyncClient, DISABLED_GetIp )
{
    AsyncClient client;
    {
        nx::utils::promise< HostAddress > prom;
        client.externalIp( URL,
                     [&]( const HostAddress& v ) { prom.set_value( v ); } );
        EXPECT_EQ( prom.get_future().get().toString(), EXTERNAL_IP );
    }
    {
        nx::utils::promise< AsyncClient::MappingInfo > prom;
        client.getMapping( URL, 8877, TCP,
                     [&]( const AsyncClient::MappingInfo& mapping )
                     { prom.set_value( mapping ); } );

        // no such mapping
        EXPECT_FALSE( prom.get_future().get().isValid() );
    }
    {
        nx::utils::promise< bool > prom;
        client.deleteMapping( URL, 8877, TCP,
                     [&]( bool v ) { prom.set_value( v ); } );
        EXPECT_FALSE( prom.get_future().get() ); // no such mapping
    }
    {
        nx::utils::promise< AsyncClient::MappingList > prom;
        client.getAllMappings( URL,
                     [&]( const AsyncClient::MappingList& v ) { prom.set_value( v ); } );
        EXPECT_EQ( prom.get_future().get().size(), 2U ); // mappings from Skype ;)
    }
}

// TODO: implement over test HTTP server
TEST( UpnpAsyncClient, DISABLED_Mapping )
{
    AsyncClient client;
    {
         nx::utils::promise< bool > prom;
         client.addMapping( URL, INTERNAL_IP, 80, 80, TCP, DESC, 0,
                      [&]( bool v ) { prom.set_value( v ); } );

         EXPECT_FALSE( prom.get_future().get() ); // wrong port
    }
    {
        nx::utils::promise< bool > prom;
        client.addMapping( URL, INTERNAL_IP, 80, 8877, TCP, DESC, 0,
                     [&]( bool v ) { prom.set_value( v ); } );

        ASSERT_TRUE( prom.get_future().get() );
    }
    {
        nx::utils::promise< SocketAddress > prom;
        client.getMapping( URL, 8877, TCP,
                     [&]( const AsyncClient::MappingInfo& m )
                     { prom.set_value( SocketAddress( m.internalIp,
                                                      m.internalPort ) ); } );

        EXPECT_EQ( prom.get_future().get().toString(),
                   SocketAddress( INTERNAL_IP, 80 ).toString() );
    }
    {
        nx::utils::promise< bool > prom;
        client.deleteMapping( URL, 8877, TCP,
                     [&]( bool v ) { prom.set_value( v ); } );

        EXPECT_TRUE( prom.get_future().get() );
    }
}

} // namespace test
} // namespace nx_upnp
