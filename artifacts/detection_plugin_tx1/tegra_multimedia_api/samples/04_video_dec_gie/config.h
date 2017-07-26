#pragma once

#include <nx/kit/ini_config.h>

struct DetectionPluginConfig: public nx::kit::IniConfig
{
    DetectionPluginFlagConfig(): nx::kit::IniConfig("detection_plugin") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");
    NX_INI_FLAG(0, enableTime, "");
    NX_INI_FLAG(0, enableFps, "");

    NX_INI_INT(1, decodersCount, "Number of simultaneous hardware video decoders.");

    NX_INI_INT(1, tegraVideoCount, "Number of simultaneous DetectionPlugin instances.");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/bin/nvidia_models/pednet.prototxt",
        deployFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/bin/nvidia_models/pednet.caffemodel",
        modelFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/var/cuda_engine.cache",
        cacheFile,
        "");

    NX_INI_STRING("", substituteFramesFilePrefix, "");

    NX_INI_INT(-1, cropRectX, "-1 means 0.");
    NX_INI_INT(-1, cropRectY, "-1 means 0.");
    NX_INI_INT(-1, cropRectW, "-1 means taking frame width.");
    NX_INI_INT(-1, cropRectH, "-1 means taking frame height.");
    NX_INI_INT(-1, maxInferenceFps, "-1 means unlimited. This value should be divided by 10");
};

inline DetectionPluginConfig& ini()
{
    static DetectionPluginConfig ini;
    return ini;
}
