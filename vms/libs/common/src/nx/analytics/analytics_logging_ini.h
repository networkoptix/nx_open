#pragma once

#include <nx/kit/ini_config.h>

namespace nx::analytics {

struct LoggingIni: public nx::kit::IniConfig
{
    LoggingIni(): IniConfig("analytics_logging.ini") { reload(); }

    NX_INI_STRING("", analyticsLogPath,
        "Path (absolute or relative to .ini dir) to a dir where all analytics logs will be\n"
        "written. If empty, no logging is performed.");

    NX_INI_FLAG(1, logObjectMetadataDetails,
        "If set to 1 additional information about objects (uuid, bounding boxes) will be logged");

    bool isLoggingEnabled() const
    {
        return analyticsLogPath[0] != '\0';
    }
};

inline LoggingIni& loggingIni()
{
    static LoggingIni ini;
    return ini;
}

} // namespace nx::analytics
