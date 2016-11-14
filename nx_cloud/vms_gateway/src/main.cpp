/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include <libvms_gateway_main.h>

#include <nx/network/socket_global.h>


int main( int argc, char* argv[] )
{
    nx::network::SocketGlobals::InitGuard sgGuard;
    nx::network::SocketGlobals::outgoingTunnelPool().designateSelfPeerId("gw", QnUuid::createUuid());
    return libVmsGatewayMain( argc, argv );
}
