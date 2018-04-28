#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/metadata/pixel_format.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

struct Ini: public nx::kit::IniConfig
{
    const std::string needUncompressedVideoFramesDescription =
        "Respective capability in manifest: one of "
            + nx::sdk::metadata::allPixelFormatsToStdString(", ") + ".\n"
        "Empty means no such capability.";

    Ini(): IniConfig("stub_metadata_plugin.ini") { reload(); }

    NX_INI_FLAG(1, enableOutput, "");
    NX_INI_STRING("", needUncompressedVideoFrames, needUncompressedVideoFramesDescription.c_str());
    NX_INI_FLAG(1, generateObjects, "");
    NX_INI_FLAG(1, generateEvents, "");
    NX_INI_INT(1, generateObjectsEveryNFrames, "");
    NX_INI_FLAG(1, generatePreviewAttributes, "");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
