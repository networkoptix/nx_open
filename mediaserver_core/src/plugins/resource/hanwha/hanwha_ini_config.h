#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct HanwhaIni: public nx::kit::IniConfig
{
    HanwhaIni(): IniConfig("hanwha.ini") { reload(); }

    NX_INI_FLAG(0, enableEdge, "Enable import from SD card.");
    NX_INI_FLAG(0, disableBypass, "Disable bypass for all NVRs.");
    NX_INI_FLAG(0, forceLensControl, "Force lens control for any Hanwha camera.");
    NX_INI_STRING(
        "",
        enabledAdvancedParameters,
        "Comma separated list of advanced parameters that will be shown on the 'Advanced' page"
        "(if the camera supports it). Other parameters will be filtered out."
        "If empty then no filter is applied.");

    NX_INI_FLAG(1, initMedia, "Enable/disable media initialization.");
    NX_INI_FLAG(1, initIo, "Enable/disable IO initialization.");
    NX_INI_FLAG(1, initAdvancedParameters, "Enable/disable advanced parameter initialization.");
    NX_INI_FLAG(1, initPtz, "Enable/disable PTZ initialization.");
    NX_INI_FLAG(1, initTwoWayAudio, "Enable/disable two-way audio initialization.");

    NX_INI_STRING(
        "",
        forcedPrimaryStreamUrl,
        "Force use of the given URL for the primary stream.");

    NX_INI_STRING(
        "",
        forcedSecondaryStreamUrl,
        "Force use of the given URL for the secondary stream.");

};

inline HanwhaIni& ini()
{
    static HanwhaIni ini;
    return ini;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
