#include <QtCore/QCoreApplication>

#include <nx/network/socket_global.h>
#include <nx/utils/test_support/run_test.h>

#include <utils/db/test_support/test_with_db_helper.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    return nx::utils::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            if (const auto value = args.get("tmp"))
                nx::db::test::TestWithDbHelper::setTemporaryDirectoryPath(*value);
        },
        nx::network::InitializationFlags::disableUdt);
}
