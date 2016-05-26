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


void parseArgs(int argc, char **argv);

int main(int argc, char **argv)
{
    nx::network::SocketGlobalsHolder socketGlobalsInstance;
    ::testing::InitGoogleTest(&argc, argv);

    parseArgs(argc, argv);

    const int result = RUN_ALL_TESTS();
    return result;
}

void parseArgs(int argc, char **argv)
{
    using namespace nx::cloud::gateway;

    std::multimap<QString, QString> args;
    parseCmdArgs(argc, argv, &args);

    const auto logIter = args.find("log");
    if (logIter != args.end())
        QnLog::initLog(logIter->second);

    const auto tmpIter = args.find("tmp");
    if (tmpIter != args.end())
        VmsGatewayFunctionalTest::setTemporaryDirectoryPath(tmpIter->second);
}
