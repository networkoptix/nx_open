// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "build_info_statistics_module.h"

#include <nx/build_info.h>

namespace nx::vms::client::desktop {

QnStatisticValuesHash BuildInfoStatisticsModule::values() const
{
    return {
        {"version", nx::build_info::vmsVersion()},
        {"revision", nx::build_info::revision()},
        {"arch", nx::build_info::applicationArch()},
        {"platform", nx::build_info::applicationPlatformNew()},
        {"platform_modification", nx::build_info::applicationPlatformModification()},
    };
}

} // namespace nx::vms::client::desktop
