#include <gtest/gtest.h>
#include <QCoreApplication>

#include <nx/network/ssl_socket.h>
#include <nx/network/test_support/run_test.h>
#include <test_support/vms_gateway_functional_test.h>

int main(int argc, char** argv)
{
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            nx::network::ssl::Engine::useRandomCertificate("vms_gateway_ut");
            if (const auto value = args.get("tmp"))
                nx::utils::TestOptions::setTemporaryDirectoryPath(*value);

            return nx::utils::test::DeinitFunctions();
        });
}
