#define USE_GMOCK
#include <nx/network/test_support/run_test.h>
#include <utils/common/app_info.h>

int main(int argc, char** argv)
{
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            return nx::utils::test::DeinitFunctions();
        });
}
