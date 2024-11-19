// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QCoreApplication>

#include <nx/network/test_support/run_test.h>
#include <nx/vms/common/application_context.h>

using namespace nx::vms::common;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    ApplicationContext appContext;

    return nx::network::test::runTest(argc, argv,
        [&](const nx::ArgumentParser& /*args*/)
        {
            return nx::utils::test::DeinitFunctions();
        });
}
