/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>
#include <utils/common/command_line_parser.h>

#include "functional_tests/test_setup.h"


void parseArgs(int argc, char **argv);

int main( int argc, char **argv )
{
    Q_INIT_RESOURCE(cloud_db_ut);

    srand(::time(NULL));

	nx::network::SocketGlobals::InitGuard sgGuard;

    ::testing::InitGoogleMock(&argc, argv);

    parseArgs(argc, argv);

    const int result = RUN_ALL_TESTS();
    return result;
}

void parseArgs(int argc, char **argv)
{
    nx::utils::ArgumentParser args(argc, argv);
    QnLog::applyArguments(args);
    nx::network::SocketGlobals::applyArguments(args);

    if (const auto value = args.get("tmp"))
        nx::cdb::CdbFunctionalTest::setTemporaryDirectoryPath(*value);

    //reading db connection options
    nx::db::ConnectionOptions connectionOptions;
    args.read("db/driverName", &connectionOptions.driverName);
    args.read("db/hostName", &connectionOptions.hostName);
    args.read("db/port", &connectionOptions.port);
    args.read("db/name", &connectionOptions.dbName);
    args.read("db/userName", &connectionOptions.userName);
    args.read("db/password", &connectionOptions.password);
    args.read("db/connectOptions", &connectionOptions.connectOptions);
    args.read("db/maxConnections", &connectionOptions.maxConnectionCount);
    nx::cdb::CdbFunctionalTest::setDbConnectionOptions(std::move(connectionOptions));
}
