#include <libtraffic_relay_main.h>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>

int main(int argc, char* argv[])
{
    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);
    return nx::cloud::relay::trafficRelayMain(argc, argv);
}
