#include <QtCore/QCoreApplication>

#include <nx/network/ssl/ssl_engine.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            nx::network::ssl::Engine::useRandomCertificate("vms_cloud_integration_tests");
            return nx::utils::test::DeinitFunctions();
        });
}
