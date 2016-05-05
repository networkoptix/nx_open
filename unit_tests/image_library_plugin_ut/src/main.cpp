/**********************************************************
* 05 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include <gtest/gtest.h>

#include <nx/network/socket_global.h>

int main( int argc, char** argv )
{
	nx::network::SocketGlobals::InitGuard sgGuard;
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
