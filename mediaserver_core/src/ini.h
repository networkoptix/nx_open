#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("mediaserver.ini") { reload(); }

    NX_INI_FLAG(f, verboseAutoRequestForwarder, "Set log level to Verbose for AutoRequestForwarder.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
