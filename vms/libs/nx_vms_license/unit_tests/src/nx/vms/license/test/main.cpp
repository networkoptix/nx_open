// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    const auto extraInit = nullptr;
    const auto socketGlobalsFlags = nx::network::InitializationFlags::disableCloudConnect;
    const auto gtestRunFlags = nx::utils::test::TestRunFlag::throwOnFailure;
    return nx::network::test::runTest(argc, argv, extraInit, socketGlobalsFlags, gtestRunFlags);
}
