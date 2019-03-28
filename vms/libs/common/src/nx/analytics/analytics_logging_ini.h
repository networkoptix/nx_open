#pragma once

#include <nx/kit/ini_config.h>

namespace nx::analytics {

struct LoggingIni: public nx::kit::IniConfig
{
    LoggingIni(): IniConfig("analytics_logging.ini") { reload(); }

    NX_INI_STRING("", analyticsLogPath, "Path where all analytics logs will be written. If\n"
        "no logging is performed.");

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
