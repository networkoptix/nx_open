#include <gtest/gtest.h>

#include <nx/network/test_support/run_test.h>

int main(int argc, char* argv[])
{
    nx::utils::TestOptions::setModuleName("nx_discovery_api_client_ut");

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::none,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);
}
