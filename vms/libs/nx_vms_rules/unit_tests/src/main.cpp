// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms/rules/metatypes.h>
#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    nx::vms::rules::Metatypes::initialize();

    return nx::utils::test::runTest(argc, argv);
}
