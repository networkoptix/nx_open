/**********************************************************
* May 19, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <utils/common/command_line_parser.h>
#include <test_support/vms_gateway_functional_test.h>

int main(int argc, char **argv)
{
    nx::network::SocketGlobalsHolder socketGlobalsInstance;
    ::testing::InitGoogleTest(&argc, argv);

    nx::utils::ArgumentParser args(argc, argv);
    QnLog::applyArguments(args);
    if (const auto value = args->get("tmp"))
    {
        nx::cloud::gateway::VmsGatewayFunctionalTest::
            setTemporaryDirectoryPath(value);
    }

    const int result = RUN_ALL_TESTS();
    return result;
}
