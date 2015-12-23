#ifndef MEDIATOR_MOCKS
#define MEDIATOR_MOCKS

#include <gmock.h>

#include <listening_peer_pool.h>
#include <mediaserver_api.h>

#include "custom_printers.h"

namespace nx {
namespace hpm {
namespace test {

class CloudDataProviderMock
        : public AbstractCloudDataProvider
{
public:
    MOCK_CONST_METHOD1( getSystem, boost::optional< System >( const String& ) );

    CloudDataProviderMock()
    {
        ON_CALL( *this, getSystem( ::testing::_ ) )
            .WillByDefault( ::testing::Return( boost::none ) );
    }

    inline
    void expect_getSystem( const String& systemId, const String& authKey, size_t count = 1 )
    {
        EXPECT_CALL( *this, getSystem( systemId ) )
            .Times( count ).WillRepeatedly( ::testing::Return( System( authKey, true ) ) );
    };
};

class MediaserverApiMock
        : public MediaserverApiBase
{
public:
    MOCK_METHOD3( pingServer, void( const SocketAddress&, const String&,
                                    std::function< void( SocketAddress, bool ) > ) );

    MediaserverApiMock( AbstractCloudDataProvider* cloudData,
                        stun::MessageDispatcher* dispatcher )
        : MediaserverApiBase( cloudData, dispatcher ) {}

    inline
    void expect_pingServer( const SocketAddress& address, const String& serverId,
                            bool result, size_t count = 1 )
    {
        EXPECT_CALL( *this, pingServer( address, serverId, ::testing::_ ) )
            .Times( count ).WillRepeatedly( ::testing::Invoke(
                [ = ]( const SocketAddress&, const String&,
                    std::function< void( SocketAddress, bool ) > onPinged )
                { onPinged( address, result ); } ) );
    }
};

} // namespace test
} // namespace hpm
} // namespase nx

#endif // MEDIATOR_MOCKS
