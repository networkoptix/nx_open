// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.

#include <iostream>

#include <nx/kit/test.h>

#include "test_disabled_ini_config.h"

extern "C" {
    #include "test_ini_config_c.h"
} // extern "C"

int main()
{
    int failedTestsCount = 0;

    failedTestsCount += nx::kit::test::runAllTests();
    failedTestsCount += test_disabled_ini_config();
    failedTestsCount += test_ini_config_c();

    std::cerr << std::endl;

    if (failedTestsCount == 0)
        std::cerr << "SUCCESS: All test suites PASSED." << std::endl;
    else
        std::cerr << failedTestsCount << " test(s) FAILED. See the messages above." << std::endl;

    return failedTestsCount;
}
