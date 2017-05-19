#include <nx/network/ssl_socket.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            nx::network::ssl::Engine::useRandomCertificate("cloud_connect_ut");
            return nx::utils::test::DeinitFunctions();
        });
}
