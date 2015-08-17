#include <gtest/gtest.h>

#include <mediaserver_api.h>

#include <common/common_globals.h>

namespace nx {
namespace hpm {
namespace test {

TEST( MediaserverApi, DISABLED_Hardcode )
{
    stun::MessageDispatcher dispatcher;
    MediaserverApi api( &dispatcher );

    EXPECT_TRUE( api.pingServer(
        lit("localhost:7001"), lit("{0000000F-8aeb-7d56-2bc7-67afae00335c}") ) );

    EXPECT_FALSE( api.pingServer( // wrong port
        lit("localhost:7002"), lit("{0000000F-8aeb-7d56-2bc7-67afae00335c}") ) );

    EXPECT_FALSE( api.pingServer( // wrong uuid
        lit("localhost:7001"), lit("{1000000F-8aeb-7d56-2bc7-67afae00335c}") ) );

}

} // namespace text
} // namespace hpm
} // namespace nx
