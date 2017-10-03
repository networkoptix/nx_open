#include <QCoreApplication>

#include <QDateTime>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/auth_tools.h>

#include "functional_tests/test_setup.h"

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    // QCoreApplication::applicationdirPath() is used throughout code (common, appserver2, etc...)
    QCoreApplication application(argc, argv);

    Q_INIT_RESOURCE(cloud_db_ut);
    const auto resultCode = nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            nx::utils::db::ConnectionOptions connectionOptions;
            QString driverName;
            args.read("db/driverName", &driverName);
            args.read("db/hostName", &connectionOptions.hostName);
            args.read("db/port", &connectionOptions.port);
            args.read("db/name", &connectionOptions.dbName);
            args.read("db/userName", &connectionOptions.userName);
            args.read("db/password", &connectionOptions.password);
            args.read("db/connectOptions", &connectionOptions.connectOptions);
            args.read("db/maxConnections", &connectionOptions.maxConnectionCount);
            if (!driverName.isEmpty())
            {
                connectionOptions.driverType = nx::utils::db::rdbmsDriverTypeFromString(
                    driverName.toStdString().c_str());
            }

            nx::cdb::test::CdbFunctionalTest::setDbConnectionOptions(
                std::move(connectionOptions));

            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::disableUdt);

    return resultCode;
}
