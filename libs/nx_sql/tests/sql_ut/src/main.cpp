#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    nx::utils::TestOptions::setModuleName("nx_sql_ut");
    return nx::utils::test::runTest(argc, argv);
}
