#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct HanwhaIni: public nx::kit::IniConfig
{
    HanwhaIni(): IniConfig("hanwha.ini") { reload(); }

    NX_INI_FLAG(0, enableEdge, "Enable import from SD card.");
    NX_INI_FLAG(0, disableBypass, "Disable bypass for all NVRs");
};

inline HanwhaIni& ini()
{
    static HanwhaIni ini;
    return ini;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
