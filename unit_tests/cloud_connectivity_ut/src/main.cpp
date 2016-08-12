#include <nx/network/socket_global.h>

#define USE_GMOCK
#include <nx/utils/test_support/run_test.h>

int main(int argc, char **argv)
{
    return nx::utils::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            nx::network::SocketGlobals::mediatorConnector().mockupAddress(
                SocketAddress::anyAddress);
        });
}
