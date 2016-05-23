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
}
