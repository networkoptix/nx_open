#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <nx/utils/test_support/sync_queue.h>

#include "mediator_mocks.h"

namespace nx {
namespace hpm {
namespace test {

TEST( MediaserverEndpointTester, DISABLED_Hardcode )
{
    nx::network::stun::MessageDispatcher dispatcher;
    CloudDataProviderMock cloud;
    MediaserverEndpointTester api( &cloud, &dispatcher );
    nx::utils::TestSyncMultiQueue< nx::network::SocketAddress, bool > results;

    // 1. success
    api.pingServer( lit("localhost:7001"),
                    "0000000F-8aeb-7d56-2bc7-67afae00335c",
                    results.pusher() );

    const auto r1 = results.pop();
    EXPECT_EQ( r1.first, nx::network::SocketAddress( lit("localhost:7001") ) );
    EXPECT_EQ( r1.second, true );

    // 2. wrong port
    api.pingServer( lit("localhost:7002"),
                    "0000000F-8aeb-7d56-2bc7-67afae00335c",
                    results.pusher() );

    const auto r2 = results.pop();
    EXPECT_EQ( r2.first, nx::network::SocketAddress( lit("localhost:7002") ) );
    EXPECT_EQ( r2.second, false );

    // 3. wrong uuid
    api.pingServer( lit("localhost:7001"),
                    "1000000F-8aeb-7d56-2bc7-67afae00335c",
                    results.pusher() );

    const auto r3 = results.pop();
    EXPECT_EQ( r3.first, nx::network::SocketAddress( lit("localhost:7001") ) );
    EXPECT_EQ( r3.second, false );
}

} // namespace text
} // namespace hpm
} // namespace nx
