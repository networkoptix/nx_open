#include <gtest/gtest.h>
#include <QCoreApplication>

#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/utils/test_support/run_test.h>

#include <test_support/vms_gateway_functional_test.h>

int main(int argc, char **argv)
{
    return nx::utils::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            nx::network::SslEngine::useRandomCertificate("vms_gateway_ut");
            if (const auto value = args.get("tmp"))
            {
                nx::cloud::gateway::VmsGatewayFunctionalTest::
                    setTemporaryDirectoryPath(*value);
            }
        });
}
