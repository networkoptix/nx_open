#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace vms::server {
namespace plugins {

struct HanwhaIni: public nx::kit::IniConfig
{
    HanwhaIni(): IniConfig("vms_server_hanwha.ini") { reload(); }

    NX_INI_FLAG(0, enableEdge, "Enable import from SD card.");
    NX_INI_FLAG(0, disableBypass, "Disable bypass for all NVRs.");
    NX_INI_FLAG(0, forceLensControl, "Force lens control for any Hanwha camera.");

    NX_INI_STRING("", enabledAdvancedParameters,
        "Comma separated list of advanced parameters that will be shown on the 'Advanced' page\n"
        "(if the camera supports it). Other parameters will be filtered out.\n"
        "If empty then no filter is applied.");

    NX_INI_FLAG(1, initMedia, "Enable/disable media initialization.");
    NX_INI_FLAG(1, initIo, "Enable/disable IO initialization.");
    NX_INI_FLAG(1, initAdvancedParameters, "Enable/disable advanced parameter initialization.");
    NX_INI_FLAG(1, initPtz, "Enable/disable PTZ initialization.");
    NX_INI_FLAG(1, initTwoWayAudio, "Enable/disable two-way audio initialization.");

    NX_INI_FLAG(0, deleteProfilesOnInitIfNeeded,
        "Allow encoder profiles deletion if there is no enouth space to create our profiles.");

    NX_INI_STRING("", forcedPrimaryStreamUrl,
        "Force use of the given URL for the primary stream.");

    NX_INI_STRING("", forcedSecondaryStreamUrl,
        "Force use of the given URL for the secondary stream.");

    NX_INI_FLAG(1, enableSingleSeekPerGroup,
        "Send single PLAY request on archive seek operation for NVRs.");

    NX_INI_FLAG(1, enableArchivePositionExtrapolation,
        "Enable archive position extrapolation when no data is received from NVR during\n"
        "some period.");

    NX_INI_INT(0, forcedRtspPort, "Forces RTSP port for Hanwha cameras.");

    NX_INI_FLAG(0, showAlternativePtzControlsOnTile,
        "Show alternative PTZ controls on tile.");

    NX_INI_FLAG(1, allowNormalizedPtzSpeed,
        "Allows normalized speed usage for continuous movement\n"
        "(if it is supported by the camera).");

    NX_INI_INT(5, chunkReaderResponseTimeoutS, "Chunk response read timeout is seconds.");
    NX_INI_INT(30, chunkReaderMessageBodyTimeoutS, "Chunk message body read timeout is seconds.");
};

inline HanwhaIni& ini()
{
    static HanwhaIni ini;
    return ini;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
