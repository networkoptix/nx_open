#include <qcoreapplication.h>

#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    nx::utils::TestOptions::setModuleName("nx_vms_client_desktop_ut");
    return nx::utils::test::runTest(argc, argv);
}
