#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/test_support/run_test.h>

#define USE_GMOCK

#include <test_support/utils.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& parser)
        {
            if (auto tmpDir = parser.get("tmp"))
            {
                nx::ut::cfg::configInstance().tmpDir = *tmpDir;
            }
            else
            {
                nx::ut::cfg::configInstance().tmpDir = 
                    QDir(QDir::tempPath()).absoluteFilePath("vms_cloud_integration_tests");
            }

            nx::network::ssl::Engine::useRandomCertificate("vms_cloud_integration_tests");
            return nx::utils::test::DeinitFunctions();
        });
}
