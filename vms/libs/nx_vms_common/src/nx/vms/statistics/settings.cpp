// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

namespace nx::vms::statistics {

namespace {

// Hardcoded credentials (because of no way to keep it better)
const QString kDefaultStatisticsServer = NX_STATISTICS_SERVER_URL"";
const std::string kDefaultUser = NX_STATISTICS_SERVER_USER"";
const std::string kDefaultPassword = NX_STATISTICS_SERVER_PASSWORD"";
const QString kDefaultCrashReportApiUrl = NX_CRASH_REPORT_API_URL"";

} // namespace

QString defaultStatisticsServer()
{
    return kDefaultStatisticsServer;
}

const std::string& defaultUser()
{
    return kDefaultUser;
}

const std::string& defaultPassword()
{
    return kDefaultPassword;
}

QString defaultCrashReportApiUrl()
{
    return kDefaultCrashReportApiUrl;
}

} // namespace nx::vms::statistics
