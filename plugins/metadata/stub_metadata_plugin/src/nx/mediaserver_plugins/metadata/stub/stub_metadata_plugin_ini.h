#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_metadata_plugin.ini") { reload(); }

    NX_INI_FLAG(1, enableOutput, "");
    NX_INI_FLAG(0, needDeepCopyOfVideoFrames, "Respective capability in manifest.");
    NX_INI_FLAG(0, needUncompressedVideoFrames, "Respective capability in manifest.");
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
