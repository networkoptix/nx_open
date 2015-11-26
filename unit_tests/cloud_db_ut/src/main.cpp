/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/tool/log/log.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>

int main( int argc, char **argv )
{
	nx::SocketGlobals::InitGuard sgGuard;
    ::testing::InitGoogleTest(&argc, argv);

    const int result = RUN_ALL_TESTS();
    return result;
}
