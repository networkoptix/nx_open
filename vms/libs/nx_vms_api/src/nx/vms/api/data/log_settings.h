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

namespace nx::vms::api {

struct NX_VMS_API LevelFilter
{
    /**%apidoc String or regex to match the tag of a log message. */
    QString filter;

    /**%apidoc The highest level of matched messages to allow into the log. */
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

    /**%apidoc The highest level of messages (that don't match any customFilters) to allow into the log. */
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
    QnUuid id;

    /**%apidoc[readonly] Log directory on the filesystem. */
    QString directory;

    /**%apidoc Maximum size (in bytes) of a single log file. */
    int maxFileSize = nx::utils::log::kDefaultMaxLogFileSize;

    /**%apidoc Maximum number of rotated log files to keep. */
    int maxBackupCount = nx::utils::log::kDefaultLogArchiveCount;

    /**%apidoc MAIN log settings. */
    LogSettings mainLog;

    /**%apidoc HTTP log settings. */
    LogSettings httpLog;

    /**%apidoc EC2 Transactions log settings. */
    LogSettings transactionLog;

    /**%apidoc HWID log settings. */
    LogSettings systemLog;

    /**%apidoc PERMISSIONS log settings. */
    LogSettings permissionsLog;

    QnUuid getId() const { return id; }

    LogSettings& logSettings(nx::log::LogName id)
    {
        switch (id)
        {
            case nx::log::LogName::main:
                return mainLog;
            case nx::log::LogName::http:
                return httpLog;
            case nx::log::LogName::transaction:
                return transactionLog;
            case nx::log::LogName::system:
                return systemLog;
            case nx::log::LogName::permissions:
                return permissionsLog;
        }
        NX_ASSERT(false);
        return mainLog;
    }
};
#define ServerLogSettings_Fields \
    (id)(maxFileSize)(maxBackupCount)(directory)\
    (mainLog)(httpLog)(transactionLog)(systemLog)(permissionsLog)
QN_FUSION_DECLARE_FUNCTIONS(ServerLogSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerLogSettings, ServerLogSettings_Fields)

} // namespace nx::vms::api
