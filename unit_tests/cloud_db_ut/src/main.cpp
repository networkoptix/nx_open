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
	nx::network::SocketGlobals::InitGuard sgGuard;

    ::testing::InitGoogleMock(&argc, argv);

    parseArgs(argc, argv);

    const int result = RUN_ALL_TESTS();
    return result;
}

void parseArgs(int argc, char **argv)
{
    std::multimap<QString, QString> args;
    parseCmdArgs(argc, argv, &args);

    const auto logIter = args.find("log");
    if (logIter != args.end())
        QnLog::initLog(logIter->second);

    const auto tmpIter = args.find("tmp");
    if (tmpIter != args.end())
        nx::cdb::CdbFunctionalTest::setTemporaryDirectoryPath(tmpIter->second);

    //reading db connection options
    nx::db::ConnectionOptions connectionOptions;

    const auto dbDriverNameIter = args.find("db/driverName");
    if (dbDriverNameIter != args.end())
        connectionOptions.driverName = dbDriverNameIter->second;
    const auto dbHostNameIter = args.find("db/hostName");
    if (dbHostNameIter != args.end())
        connectionOptions.hostName = dbHostNameIter->second;
    const auto dbPortIter = args.find("db/port");
    if (dbPortIter != args.end())
        connectionOptions.port = dbPortIter->second.toInt();
    const auto dbNameIter = args.find("db/name");
    if (dbNameIter != args.end())
        connectionOptions.dbName = dbNameIter->second;
    const auto dbUserNameIter = args.find("db/userName");
    if (dbUserNameIter != args.end())
        connectionOptions.userName = dbUserNameIter->second;
    const auto dbPasswordIter = args.find("db/password");
    if (dbPasswordIter != args.end())
        connectionOptions.password = dbPasswordIter->second;
    const auto dbConnectOptionsIter = args.find("db/connectOptions");
    if (dbConnectOptionsIter != args.end())
        connectionOptions.connectOptions = dbConnectOptionsIter->second;
    const auto dbMaxConnectionsIter = args.find("db/maxConnections");
    if (dbMaxConnectionsIter != args.end())
        connectionOptions.maxConnectionCount = dbMaxConnectionsIter->second.toInt();

    nx::cdb::CdbFunctionalTest::setDbConnectionOptions(std::move(connectionOptions));
}
