// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCoreApplication>

#include <nx/utils/test_support/run_test.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/metatypes.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    return nx::utils::test::runTest(
        argc,
        argv,
        [](const nx::utils::ArgumentParser& args)
        {
            nx::vms::rules::Metatypes::initialize();

            auto iniTweaks = std::make_unique<nx::vms::rules::Ini::Tweaks>();
            iniTweaks->set(&nx::vms::rules::ini().rulesEngine, "new");

            nx::utils::test::DeinitFunctions deinitFunctions;
            deinitFunctions.push_back(
                [iniTweaks = std::move(iniTweaks)]() mutable { iniTweaks.reset(); });
            return deinitFunctions;
        },
        nx::utils::test::TestRunFlag::throwOnFailure);
}
