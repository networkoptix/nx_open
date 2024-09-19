// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::statistics {

// Hardcoded credentials (because of no way to keep it better)
const QString kDefaultStatisticsServer = NX_STATISTICS_SERVER_URL"";
const QString kDefaultUser = NX_STATISTICS_SERVER_USER"";
const QString kDefaultPassword = NX_STATISTICS_SERVER_PASSWORD"";
const QString kDefaultCrashReportApiUrl = NX_CRASH_REPORT_API_URL"";

} // namespace nx::vms::statistics
