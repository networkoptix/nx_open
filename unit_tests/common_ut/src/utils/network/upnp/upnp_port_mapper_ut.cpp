#include "upnp_port_mapper_mocked.h"

#include <common/common_globals.h>
#include <api/global_settings.h>

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

TEST( UpnpPortMapper, NormalUsage )
{
    TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001 and 80
    SyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( info ); } ) );

    EXPECT_EQ( queue7001.pop().toString(), "12.34.56.78:7001" );
    EXPECT_EQ( clientMock.mappings().size(), 1 );

    const auto addr7001 = clientMock.mappings()[ tcpPort( 7001 ) ].first;
    EXPECT_EQ( addr7001.toString(), lit( "192.168.0.10:7001" ) );

    SyncQueue< SocketAddress > queue80;
    EXPECT_TRUE( portMapper.enableMapping( 80, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue80.push( info ); } ) );

    const auto map80 = queue80.pop();
    EXPECT_EQ( map80.address.toString(), lit("12.34.56.78") );
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
    TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ) );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // simulate mapping 6666 -> 192.168.0.10:7001
    EXPECT_TRUE( clientMock.mkMapping( std::make_pair(
        std::make_pair( 6666, PortMapper::Protocol::TCP ),
        std::make_pair( SocketAddress( lit( "192.168.0.10" ), 7001 ), QString() ) ) ) );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );

    SyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( std::move( info ) ); } ) );

    // existed mapping should be in use
    EXPECT_EQ( queue7001.pop().toString(), lit("12.34.56.78:6666") );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );

    // existed mapping should be removed when if need
    EXPECT_TRUE( portMapper.disableMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0 );
}

TEST( UpnpPortMapper, CheckMappings )
{
    TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper( lit( "192.168.0.10" ), lit( "12.34.56.78" ), 500 );
    AsyncClientMock& clientMock = portMapper.clientMock();

    // Map 7001
    SyncQueue< SocketAddress > queue7001;
    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress info )
                 { queue7001.push( std::move(info) ); } ) );

    EXPECT_EQ( queue7001.pop().toString(), "12.34.56.78:7001" );
    EXPECT_EQ( clientMock.mappingsCount(), 1 );

    EXPECT_TRUE( clientMock.rmMapping( 7001, PortMapper::Protocol::TCP ) );
    EXPECT_EQ( clientMock.mappingsCount(), 0 );

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
    TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapper portMapper( 10000 );

    EXPECT_TRUE( portMapper.enableMapping( 7001, PortMapper::Protocol::TCP,
                 [&]( SocketAddress ) {} ) );

    QThread::sleep( 60 * 60 ); // vary long time ;)
}

} // namespace test
} // namespace nx_upnp
