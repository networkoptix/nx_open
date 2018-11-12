#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>

#include <nx/cloud/cdb/lib_cloud_db_main.h>

int main( int argc, char* argv[] )
{
    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);
    return libCloudDBMain( argc, argv );
}
