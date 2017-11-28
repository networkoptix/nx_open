#include <nx/network/ssl/ssl_engine.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

#include "test_json_data.inl"

int main(int argc, char** argv)
{
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            return nx::utils::test::DeinitFunctions();
        });
}
