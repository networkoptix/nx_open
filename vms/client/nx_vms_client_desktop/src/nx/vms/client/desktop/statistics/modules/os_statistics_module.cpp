// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_statistics_module.h"

#include <QtCore/QSysInfo>

namespace nx::vms::client::desktop {

QnStatisticValuesHash OsStatisticsModule::values() const
{
    return {
        {"type", QSysInfo::productType()},
        {"version", QSysInfo::productVersion()},
        {"name", QSysInfo::prettyProductName()},
    };
}

} // namespace nx::vms::client::desktop
