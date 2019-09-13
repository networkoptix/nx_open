#include <QtCore/QCoreApplication>

#include <nx/fusion/serialization/lexical.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    return nx::network::test::runTest(
        argc, argv,
        nullptr,
        nx::network::InitializationFlags::disableUdt,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);
}
