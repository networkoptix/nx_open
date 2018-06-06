#include <QtCore/QCoreApplication>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/ssl/ssl_engine.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            nx::network::ssl::Engine::useRandomCertificate("traffic_relay_ut");
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::disableUdt,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);
}
