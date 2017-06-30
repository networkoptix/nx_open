#include <nx/network/ssl/ssl_engine.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            nx::network::ssl::Engine::useRandomCertificate("nx_network_ut");
            return nx::utils::test::DeinitFunctions();
        });
}
