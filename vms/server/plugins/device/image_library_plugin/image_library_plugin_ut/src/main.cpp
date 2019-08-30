#include <nx/utils/test_support/run_test.h>

int main( int argc, char** argv )
{
    nx::utils::TestOptions::setModuleName("nx_image_library_plugin_ut");
    return nx::utils::runTest(argc, argv);
}
