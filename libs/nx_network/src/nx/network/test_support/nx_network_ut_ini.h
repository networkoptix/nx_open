#pragma once

#include <nx/kit/ini_config.h>

namespace nx::network::test {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_network_ut.ini") { reload(); }

    NX_INI_FLAG(true, throwOnFailure, "Throw on failure.");
};

} // nx::network::test
