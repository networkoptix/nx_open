#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/common/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

struct Ini: public nx::kit::IniConfig
{
    const std::string needUncompressedVideoFramesDescription =
        "Respective capability in the manifest: one of "
            + nx::sdk::analytics::common::allPixelFormatsToStdString(", ") + ".\n"
        "Empty means no such capability.";

    Ini(): IniConfig("stub_analytics_plugin.ini") { reload(); }

    NX_INI_FLAG(1, enableOutput, "");
    NX_INI_STRING("", needUncompressedVideoFrames, needUncompressedVideoFramesDescription.c_str());
    NX_INI_FLAG(1, generateObjects, "");
    NX_INI_FLAG(1, generateEvents, "");
    NX_INI_INT(1, generateObjectsEveryNFrames, "");
    NX_INI_FLAG(1, generatePreviewAttributes, "");
    NX_INI_FLAG(0, deviceModelDependent, "Respective capability in the manifest");

    NX_INI_FLAG(1, throwPluginEventsFromEngine,
        "Periodically throw plugin events from Engine to Server");

    NX_INI_FLAG(1, throwPluginEventsFromDeviceAgent,
        "Periodically throw plugin events from DeviceAgent to Server");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
