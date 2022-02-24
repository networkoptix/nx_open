// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::analytics {

struct LoggingIni: public nx::kit::IniConfig
{
    LoggingIni(): IniConfig("analytics_logging.ini") { reload(); }

    NX_INI_STRING("", analyticsLogPath,
        "Path (absolute or relative to .ini dir) to the existing dir where all Analytics Logs\n"
        "will be written. If empty, no logging is performed.");

    NX_INI_FLAG(1, logObjectMetadataDetails,
        "If set, additional information about objects (uuid, bounding boxes) will be logged.");

    bool isLoggingEnabled() const
    {
        return analyticsLogPath[0] != '\0';
    }
};

NX_VMS_COMMON_API LoggingIni& loggingIni();

} // namespace nx::analytics
