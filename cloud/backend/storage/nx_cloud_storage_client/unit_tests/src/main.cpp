#include <QCoreApplication>

#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    const auto resultCode = nx::network::test::runTest(
        argc, argv,
        nullptr,
        nx::network::InitializationFlags::disableUdt,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);

    return resultCode;
}
