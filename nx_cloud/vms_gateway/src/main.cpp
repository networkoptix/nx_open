#include <libvms_gateway_main.h>

#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/socket_global.h>

int main( int argc, char* argv[] )
{
    nx::network::SocketGlobals::InitGuard sgGuard;
    nx::network::SocketGlobals::outgoingTunnelPool().assignOwnPeerId(
        "gw",
        QnUuid::createUuid());
    return libVmsGatewayMain( argc, argv );
}
