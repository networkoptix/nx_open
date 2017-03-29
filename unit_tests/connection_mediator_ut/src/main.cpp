#include <QtCore/QCoreApplication>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& args)
        {
            if (const auto value = args.get("tmp"))
                nx::utils::test::TestWithTemporaryDirectory::setTemporaryDirectoryPath(*value);

            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::disableUdt);
}
