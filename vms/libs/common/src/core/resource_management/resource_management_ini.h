#pragma once

#include <nx/kit/ini_config.h>

struct ResourceManagementIni: public nx::kit::IniConfig
{
    ResourceManagementIni(): IniConfig("resource_management.ini") { reload(); }

    NX_INI_INT(15'000, cameraDiscoveryIntervalMs, "Wait at least this many milliseconds between camera discovery attempts");
};

ResourceManagementIni& resourceManagementIni();
