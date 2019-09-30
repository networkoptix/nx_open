#include <QtCore/QCoreApplication>

#include <nx/sql/test_support/test_with_db_helper.h>
#include <nx/utils/deprecated_settings.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            nx::sql::ConnectionOptions connectionOptions;
            connectionOptions.driverType = nx::sql::RdbmsDriverType::sqlite;
            connectionOptions.loadFromSettings(QnSettings(args));
            nx::sql::test::TestWithDbHelper::setDbConnectionOptions(
                std::move(connectionOptions));
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::disableUdt,
        nx::utils::test::GtestRunFlag::gtestThrowOnFailure);
}
