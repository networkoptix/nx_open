#include <qcoreapplication.h>

#include <nx/network/test_support/run_test.h>

#include "test_setup.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            if (const auto value = args.get("tmp"))
                nx::utils::TestOptions::setTemporaryDirectoryPath(*value);

            return nx::utils::test::DeinitFunctions();
        });
}
