#pragma once

#include <nx/kit/ini_config.h>

namespace nx::analytics {

struct LoggingIni: public nx::kit::IniConfig
{
    LoggingIni(): IniConfig("analytics_logging.ini") { reload(); }

    NX_INI_STRING("", analyticsLogPath,
        "Path (absolute or relative to .ini dir) to a dir where all analytics logs will be\n"
        "written. If empty, no logging is performed.");

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
