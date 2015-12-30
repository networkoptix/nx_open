/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>


int main(int argc, char **argv)
{
    nx::network::SocketGlobals::InitGuard sgGuard;

    ::testing::InitGoogleMock(&argc, argv);

    const int result = RUN_ALL_TESTS();
    return result;
}
