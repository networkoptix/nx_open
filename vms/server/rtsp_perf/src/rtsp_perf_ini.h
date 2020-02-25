#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("rtsp_perf.ini") { reload(); }

    NX_INI_INT(30, printDurationToWarnMs,
        "If printing a report line to stdout takes more than this time (in milliseconds), a\n"
        "warning is logged to stderr. At the end, the maximum print duration will be logged.");

    NX_INI_FLAG(1, useAsyncReporting,
        "Print report on stdout asynchronously - put each line into a queue and print lines from\n"
        "the queue in a dedicated thread. This helps to avoid rtsp_perf blocking in case its\n"
        "report is being redirected to another tool which fails to read out lines promptly.\n"
        "At the end, the maximum queue size will be logged.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
