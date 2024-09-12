// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QCoreApplication>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/test_support/run_test.h>
#include <nx/utils/move_only_func.h>

#include <common/static_common_module.h>
#include <common/common_meta_types.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QnStaticCommonModule staticCommonModule(nx::vms::api::PeerType::server);

    return nx::network::test::runTest(argc, argv,
        [&](const nx::ArgumentParser& /*args*/)
        {
            return nx::utils::test::DeinitFunctions();
        });
}
