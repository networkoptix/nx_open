#include <qcoreapplication.h>

#include <nx/utils/test_support/run_test.h>

#include "test_setup.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return nx::utils::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            return nx::utils::test::DeinitFunctions();
        });
}
