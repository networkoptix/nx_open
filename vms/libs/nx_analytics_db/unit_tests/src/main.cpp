#include <QtCore/QCoreApplication>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            return nx::utils::test::DeinitFunctions();
        },
        0,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);
}
