#include <QtCore/QCoreApplication>

#define USE_GMOCK
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            nx::network::ssl::Engine::useRandomCertificate("cloud_connect_ut");
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::none,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);
}
