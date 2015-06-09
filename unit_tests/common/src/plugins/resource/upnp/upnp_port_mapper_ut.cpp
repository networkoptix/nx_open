#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>
#include <plugins/resource/upnp/upnp_port_mapper_internal.h>

#include <gtest.h>
#include <future>

std::pair< quint16, UpnpPortMapper::Protocol > tcpPort( quint16 port )
{
    return std::make_pair( port, UpnpPortMapper::Protocol::TCP );
}

TEST(UpnpPortMapper, Mocked)
{
    UpnpPortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    UpnpAsyncClientMock& clientMock = portMapper.clientMock();

    std::promise< bool > enable7001p;
    EXPECT_TRUE( portMapper.enableMapping( 7001, UpnpPortMapper::Protocol::TCP,
                 [&]( const UpnpPortMapper::MappingInfo& info )
    {
        EXPECT_EQ( info.internalPort, 7001 );
        EXPECT_EQ( info.externalPort, 7001 );
        EXPECT_EQ( info.externalIp, "12.34.56.78" );
        enable7001p.set_value( true );
    } ) );

    EXPECT_TRUE( enable7001p.get_future().get() );
    EXPECT_EQ( clientMock.m_mappings.size(), 1 );

    const auto addr1 = clientMock.m_mappings[ tcpPort( 7001 ) ].first;
    EXPECT_EQ( addr1.toString().toUtf8(), QByteArray( "192.168.0.10:7001" ) );

    std::promise< quint16 > enable80p;
    EXPECT_TRUE( portMapper.enableMapping( 80, UpnpPortMapper::Protocol::TCP,
                 [&]( const UpnpPortMapper::MappingInfo& info )
    {
        EXPECT_EQ( info.internalPort, 80 );
        EXPECT_EQ( info.externalIp, "12.34.56.78" );
        enable80p.set_value( info.externalPort );
    } ) );

    const auto mappedPort = enable80p.get_future().get();
    ASSERT_GT( mappedPort, 80 );
    EXPECT_EQ( clientMock.m_mappings.size(), 2 );

    EXPECT_TRUE( portMapper.disableMapping( 7001, UpnpPortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.m_mappings.size(), 1 );

    const auto addr2 = clientMock.m_mappings[ tcpPort( mappedPort ) ].first;
    EXPECT_EQ( addr2.toString().toUtf8(), QByteArray( "192.168.0.10:80" ) );

    EXPECT_TRUE( portMapper.disableMapping( mappedPort, UpnpPortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.m_mappings.size(), 0 );
}
