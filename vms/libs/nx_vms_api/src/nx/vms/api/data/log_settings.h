// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <vector>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/log/log_writers.h>
#include <nx/utils/uuid.h>

#include "map.h"

namespace nx::vms::api {

struct NX_VMS_API LevelFilter
{
    /**%apidoc[opt] String or regex to match the tag of a log message. */
    QString filter;

    /**%apidoc[opt] The highest level of matched messages to allow into the log. */
    nx::utils::log::Level level = nx::utils::log::kDefaultLevel;
};
#define LevelFilter_Fields (filter)(level)
QN_FUSION_DECLARE_FUNCTIONS(LevelFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LevelFilter, LevelFilter_Fields)

struct NX_VMS_API LogSettings
{
    /**%apidoc[readonly] Log file name on the filesystem. */
    QString fileName;

    /**%apidoc[readonly] Pre-set filters, only messages with matching tags are allowed into the log. */
    std::vector<QString> predefinedFilters;

    /**%apidoc[opt] The highest level of messages (that don't match any customFilters) to allow
     * into the log.
     */
    nx::utils::log::Level primaryLevel = nx::utils::log::kDefaultLevel;

    /**%apidoc Modifiable filters and levels for the log. */
    std::vector<LevelFilter> customFilters;

    void addPredefinedFilters(std::set<nx::utils::log::Filter> filters);
    void addCustomFilters(nx::utils::log::LevelFilters filters);
};
#define LogSettings_Fields (fileName)(predefinedFilters)(primaryLevel)(customFilters)
QN_FUSION_DECLARE_FUNCTIONS(LogSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LogSettings, LogSettings_Fields)

struct NX_VMS_API ServerLogSettings
{
    /**%apidoc[readonly] Server id. */
    nx::Uuid id;

    /**%apidoc[readonly] Log directory on the filesystem. */
    QString directory;

    /**%apidoc[opt]:integer Maximum size (in bytes) of all log files on the filesystem. */
    double maxVolumeSizeB = nx::utils::log::kDefaultMaxLogVolumeSizeB;

    /**%apidoc[opt]:integer Maximum size (in bytes) of a single log file. */
    double maxFileSizeB = nx::utils::log::kDefaultMaxLogFileSizeB;

    /**%apidoc[opt] Maximum time duration of a single log file. */
    std::chrono::seconds maxFileTimePeriodS = nx::utils::log::kDefaultMaxLogFileTimePeriodS;

    /**%apidoc[opt] Disable/enable zipping of log files on the filesystem. */
    bool archivingEnabled = nx::utils::log::kDefaultLogArchivingEnabled;

    /**%apidoc[opt] MAIN log settings. */
    LogSettings mainLog;

    /**%apidoc[opt] HTTP log settings. */
    LogSettings httpLog;

    /**%apidoc[opt] HWID log settings. */
    LogSettings systemLog;

    nx::Uuid getId() const { return id; }

    LogSettings& logSettings(nx::log::LogName logName)
    {
        switch (logName)
        {
            case nx::log::LogName::main:
                return mainLog;
            case nx::log::LogName::http:
                return httpLog;
            case nx::log::LogName::system:
                return systemLog;
        }
        NX_ASSERT(false);
        return mainLog;
    }
};
#define ServerLogSettings_Fields \
    (id)(maxFileSizeB)(maxVolumeSizeB)(maxFileTimePeriodS)(archivingEnabled)(directory)(mainLog)(httpLog)(systemLog)
QN_FUSION_DECLARE_FUNCTIONS(ServerLogSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerLogSettings, ServerLogSettings_Fields)

using ServerLogSettingsMap = Map<nx::Uuid, nx::vms::api::ServerLogSettings>;

} // namespace nx::vms::api
