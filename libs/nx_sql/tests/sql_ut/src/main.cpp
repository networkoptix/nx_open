#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    return nx::utils::test::runTest(argc, argv);
}
