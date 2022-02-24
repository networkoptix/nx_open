// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_logging_ini.h"

namespace nx::analytics {

LoggingIni& loggingIni()
{
    static LoggingIni ini;
    return ini;
}

} // namespace nx::analytics
