// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::statistics {

NX_VMS_COMMON_API QString defaultStatisticsServer();
NX_VMS_COMMON_API const std::string& defaultUser();
NX_VMS_COMMON_API const std::string& defaultPassword();
NX_VMS_COMMON_API QString defaultCrashReportApiUrl();

} // namespace nx::vms::statistics
