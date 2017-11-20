#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

/**
 * Used by libtegra_video_metadata_plugin.so.
 */
struct TegraVideoMetadataPluginIniConfig: public nx::kit::IniConfig
{
    TegraVideoMetadataPluginIniConfig(): nx::kit::IniConfig("tegra_video_metadata_plugin.ini")
    {
        reload();
    }

    NX_INI_FLAG(1, enableOutput, ""); //< TODO: #mike: Change to 0.
    NX_INI_FLAG(0, enableTime, "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/nvidia_models/carnet.prototxt",
        deployFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/nvidia_models/carnet.caffemodel",
        modelFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/var/cuda_engine.cache",
        cacheFile,
        "");

    NX_INI_INT(0, netWidth, "Net input width (pixels). If 0, try to parse from deployFile.");
    NX_INI_INT(0, netHeight, "Net input height (pixels). If 0, try to parse from deployFile.");
};

inline TegraVideoMetadataPluginIniConfig& ini()
{
    static TegraVideoMetadataPluginIniConfig ini;
    return ini;
}

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
