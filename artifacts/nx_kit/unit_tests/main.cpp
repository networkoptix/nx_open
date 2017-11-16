// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.

#include <iostream>

#include <nx/kit/test.h>

#include "disabled_ini_config_ut.h"

extern "C" {
#include "ini_config_c_ut.h"
} // extern "C"

int main()
{
    int failedTestsCount = 0;

    failedTestsCount += nx::kit::test::runAllTests();
    failedTestsCount += disabled_ini_config_ut();
    failedTestsCount += ini_config_c_ut();

    std::cerr << std::endl;

    if (failedTestsCount == 0)
        std::cerr << "SUCCESS: All test suites PASSED." << std::endl;
    else
        std::cerr << failedTestsCount << " test(s) FAILED. See the messages above." << std::endl;

    return failedTestsCount;
}
