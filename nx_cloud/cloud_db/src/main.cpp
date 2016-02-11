/**********************************************************
* Jul 16, 2015
* a.kolesnikov
***********************************************************/

#include <lib_cloud_db_main.h>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>


int main( int argc, char* argv[] )
{
	nx::network::SocketGlobals::InitGuard sgGuard;
    return libCloudDBMain( argc, argv );
}
