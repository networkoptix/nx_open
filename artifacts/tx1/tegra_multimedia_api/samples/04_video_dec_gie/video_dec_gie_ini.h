#pragma once

#include <nx/kit/ini_config.h>

/**
 * Used by video_dec_gie executable and not used by libtegra_video.so library.
 */
struct VideoDecGieIniConfig: public nx::kit::IniConfig
{
    VideoDecGieIniConfig(): nx::kit::IniConfig("video_dec_gie.ini") { reload(); }

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/nvidia_models/pednet.prototxt",
        deployFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/nvidia_models/pednet.caffemodel",
        modelFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/var/cuda_engine.cache",
        cacheFile,
        "");

    NX_INI_INT(0, netWidth, "Net input width (pixels). If 0, try to parse from deployFile.");
    NX_INI_INT(0, netHeight, "Net input height (pixels). If 0, try to parse from deployFile.");

    NX_INI_STRING("", substituteFramesFilePrefix, "If specified, frames are taken from files.");

    NX_INI_INT(1, tegraVideoCount, "Number of simultaneous TegraVideo instances.");
};

inline VideoDecGieIniConfig& exeIni()
{
    static VideoDecGieIniConfig ini;
    return ini;
}
