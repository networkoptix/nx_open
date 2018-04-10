#include <nx/utils/db/test_support/test_with_db_helper.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
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

            nx::utils::db::test::TestWithDbHelper::setDbConnectionOptions(
                std::move(connectionOptions));

            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::disableUdt,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);

    return resultCode;
}
