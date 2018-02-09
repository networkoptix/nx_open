#include <nx/network/ssl/ssl_engine.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    std::vector<char*> extendedArgs;
    for (int i = 0; i < argc; ++i)
        extendedArgs.push_back(argv[i]);
    extendedArgs.push_back("--gtest_throw_on_failure");

    argc = (int)extendedArgs.size();
    return nx::network::test::runTest(
        //argc, argv,
        argc, extendedArgs.data(),
        [](const nx::utils::ArgumentParser&)
        {
            nx::network::ssl::Engine::useRandomCertificate("nx_network_ut");
            return nx::utils::test::DeinitFunctions();
        });
}
