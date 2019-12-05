#pragma once

#include <nx/kit/ini_config.h>

namespace nx {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_system_commands.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");
};

Ini& ini();

} // namespace nx
