#include <QCoreApplication>

#include <QDateTime>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>

#include "functional_tests/test_setup.h"

#define USE_GMOCK
#include <nx/utils/test_support/run_test.h>

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(cloud_db_ut);
    return nx::utils::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            if (const auto value = args.get("tmp"))
                nx::cdb::CdbFunctionalTest::setTemporaryDirectoryPath(*value);

            nx::db::ConnectionOptions connectionOptions;
            QString driverName;
            args.read("db/driverName", &driverName);
            args.read("db/hostName", &connectionOptions.hostName);
            args.read("db/port", &connectionOptions.port);
            args.read("db/name", &connectionOptions.dbName);
            args.read("db/userName", &connectionOptions.userName);
            args.read("db/password", &connectionOptions.password);
            args.read("db/connectOptions", &connectionOptions.connectOptions);
            args.read("db/maxConnections", &connectionOptions.maxConnectionCount);
            connectionOptions.driverType =
                QnLexical::deserialized<nx::db::RdbmsDriverType>(driverName, connectionOptions.driverType);

            nx::cdb::CdbFunctionalTest::setDbConnectionOptions(
                std::move(connectionOptions));
        });
}
