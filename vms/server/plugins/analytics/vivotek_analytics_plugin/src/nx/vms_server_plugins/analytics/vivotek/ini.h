#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms_server_plugins::analytics::vivotek {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("vivotek_analytics_plugin.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");
};

Ini& ini();

} // namespace nx::vms_server_plugins::analytics::vivotek
