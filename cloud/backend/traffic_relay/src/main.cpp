#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>

#include <nx/cloud/relay/libtraffic_relay_main.h>

int main(int argc, char* argv[])
{
    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);
    return nx::cloud::relay::trafficRelayMain(argc, argv);
}
