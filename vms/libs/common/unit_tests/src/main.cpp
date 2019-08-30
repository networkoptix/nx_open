#include <qcoreapplication.h>

#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    nx::utils::TestOptions::setModuleName("common_ut");

    QCoreApplication app(argc, argv);
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            return nx::utils::test::DeinitFunctions();
        });
}
