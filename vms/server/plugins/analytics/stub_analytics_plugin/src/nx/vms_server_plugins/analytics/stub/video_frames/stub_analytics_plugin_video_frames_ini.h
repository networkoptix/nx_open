// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace video_frames {

struct Ini: public nx::kit::IniConfig
{
    const std::string needUncompressedVideoFramesDescription =
        "Respective capability in the manifest: one of "
            + nx::sdk::analytics::allPixelFormatsToStdString(", ") + ".\n"
        "Empty means no such capability.";

    Ini(): IniConfig("stub_analytics_plugin_video_frames.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");

    NX_INI_FLAG(0, deviceDependent, "Respective capability in the manifest.");

    NX_INI_STRING("", needUncompressedVideoFrames, needUncompressedVideoFramesDescription.c_str());

    NX_INI_INT(-1, crashDeviceAgentOnFrameN,
        "If >= 0, intentionally crash DeviceAgent on processing a frame with this index.");

    NX_INI_STRING("primary", preferredStream,
        "Preferred stream in the Engine manifest. Possible values: \"primary\", \"secondary\",\n"
        "\"undefined\".");
};

Ini& ini();

} // namespace video_frames
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
