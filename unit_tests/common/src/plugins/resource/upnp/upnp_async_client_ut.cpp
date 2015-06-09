#include <plugins/resource/upnp/upnp_async_client.h>

#include <common/common_globals.h>

#include <gtest.h>
#include <future>

// TODO: implement over mocked sockets
TEST(UpnpAsyncClient, DISABLED_GetIp)
{
    UpnpAsyncClient client;
    const QUrl URL( lit("http://192.168.1.1:44264/ctl/IPConn") );
    const auto TCP = UpnpAsyncClient::Protocol::TCP;

    {
        std::promise< HostAddress > prom;
        EXPECT_TRUE( client.externalIp( URL,
                     [&]( const HostAddress& v ) { prom.set_value( v ); } ) );
        EXPECT_EQ( prom.get_future().get().toString().toUtf8(), QByteArray( "10.0.2.130" ) );
    }
    {
        std::promise< UpnpAsyncClient::MappingInfo > prom;
        EXPECT_TRUE( client.getMapping( URL, 8877, TCP,
                     [&]( const UpnpAsyncClient::MappingInfo& mapping )
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
}

// TODO: implement over mocked sockets
TEST(UpnpAsyncClient, DISABLED_Mapping)
{
    UpnpAsyncClient client;
    const QUrl URL( lit("http://192.168.1.1:44264/ctl/IPConn") );
    const auto TCP = UpnpAsyncClient::Protocol::TCP;
    const QString IP = lit( "192.168.1.170" );
    const QString DESC = lit( "test" );

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
                     [&]( const UpnpAsyncClient::MappingInfo& m )
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
