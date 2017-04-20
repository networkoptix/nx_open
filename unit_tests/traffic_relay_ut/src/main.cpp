#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/auth_tools.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

#include "relay_test_setup.h"

int main(int argc, char** argv)
{
    const auto resultCode = nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            if (const auto value = args.get("tmp"))
                nx::utils::TestOptions::setTemporaryDirectoryPath(*value);

            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::disableUdt);

    return resultCode;
}
