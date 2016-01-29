/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>


int main( int argc, char **argv )
{
	nx::network::SocketGlobals::InitGuard sgGuard;

    ::testing::InitGoogleMock(&argc, argv);

    const int result = RUN_ALL_TESTS();
    return result;
}
