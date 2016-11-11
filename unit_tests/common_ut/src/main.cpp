#include <qcoreapplication.h>

#include <nx/utils/test_support/run_test.h>

#include "test_setup.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    return nx::utils::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            if (const auto value = args.get("tmp"))
                TestSetup::setTemporaryDirectoryPath(*value);
        });
}
