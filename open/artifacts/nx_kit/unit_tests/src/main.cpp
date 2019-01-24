// Copyright 2018-present Network Optix, Inc.

#include <iostream>

#include <nx/kit/test.h>
#include <nx/kit/debug.h>

#include "disabled_ini_config_ut.h"

int main(int argc, const char* const argv[])
{
    int failedTestsCount = 0;

    failedTestsCount += nx::kit::test::runAllTests("nx_kit", argc, argv);
    failedTestsCount += disabled_ini_config_ut(argc, argv);

    std::cerr << std::endl;

    if (failedTestsCount == 0)
        std::cerr << "SUCCESS: All test suites PASSED." << std::endl;
    else
        std::cerr << failedTestsCount << " test(s) FAILED. See the messages above." << std::endl;

    return failedTestsCount;
}
