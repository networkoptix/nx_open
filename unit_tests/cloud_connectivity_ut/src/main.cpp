#include <nx/network/ssl_socket.h>
#include <utils/db/test_support/test_with_db_helper.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            nx::network::SslEngine::useRandomCertificate("cloud_connect_ut");
            if (const auto value = args.get("tmp"))
                nx::db::test::TestWithDbHelper::setTemporaryDirectoryPath(*value);

            return nx::utils::test::DeinitFunctions();
        });
}
