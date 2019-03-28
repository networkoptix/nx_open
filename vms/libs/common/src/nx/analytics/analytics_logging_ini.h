#pragma once

#include <nx/kit/ini_config.h>

namespace nx::analytics {

struct LoggingIni: public nx::kit::IniConfig
{
    LoggingIni(): IniConfig("analytics_logging.ini") { reload(); }

    NX_INI_FLAG(0, enableLogging,
        "Enable logging of video frame info and metadata packets in multiple places in the\n"
        "Server and Client");
};

inline LoggingIni& loggingIni()
{
    static LoggingIni ini;
    return ini;
}

} // namespace nx::analytics
