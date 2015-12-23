#include <media_server_main.h>
#include <nx/network/socket_global.h>

int main(int argc, char* argv[])
{
	nx::network::SocketGlobals::InitGuard sgGuard;
    return mediaServerMain(argc, argv);
}
