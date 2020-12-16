// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

struct Ini: public nx::kit::IniConfig
{
    const std::string needUncompressedVideoFramesDescription =
        "Respective capability in the manifest: one of "
            + nx::sdk::analytics::allPixelFormatsToStdString(", ") + ".\n"
        "Empty means no such capability.";

    Ini(): IniConfig("stub_analytics_plugin.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");
    NX_INI_STRING("", needUncompressedVideoFrames, needUncompressedVideoFramesDescription.c_str());
    NX_INI_FLAG(0, deviceDependent, "Respective capability in the manifest.");
    NX_INI_INT(-1, crashDeviceAgentOnFrameN,
        "If >= 0, intentionally crash DeviceAgent on processing a frame with this index.");

    NX_INI_FLAG(1, needMetadata,
        "If set, Engine will declare the corresponding stream type filter in the manifest.");

    NX_INI_FLAG(0, visualizeMotion,
        "If set, the Engine will visualize motion metadata via object bounding boxes (each\n"
        "active motion grid cell is dispalyed as a separate object), regular object generation\n"
        "is disabled in this case, so the correspondent DeviceAgent settings won't work. Use\n"
        "carefully since this option can affect performance very much.");

    NX_INI_STRING("primary", preferredStream,
        "Preferred stream in the Engine manifest. Possible values: \"primary\", \"secondary\".");

    NX_INI_STRING("http://internal.server/addPerson?trackId=", addPersonActionUrlPrefix,
        "Prefix for the URL returned by addPerson action; track id will be appended to this "
        "prefix.");

    NX_INI_FLAG(0, keepObjectBoundingBoxRotation,
        "If set, Engine will declare the corresponding capability in the manifest.");

    NX_INI_FLAG(0, usePluginAsSettingsOrigin,
        "If set, Engine will declare the corresponding capability in the manifest.");

    NX_INI_INT(0, multiPluginInstanceCount,
        "If >= 1, the multi-IPlugin entry point function will produce the specified number\n"
        "of IPlugin instances, and the single-IPlugin entry point function will return null.");

    NX_INI_FLAG(1, sendSettingsModelWithValues,
        "If set, Settings Model is being sent along with setting values when\n"
        "setSettings() or getPluginSideSettings() are called.");
};

Ini& ini();

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
