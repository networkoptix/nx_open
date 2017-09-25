#pragma once

#include <nx/utils/flag_config.h>

struct TegraVideoFlagConfig: public nx::utils::FlagConfig
{
    TegraVideoFlagConfig(): nx::utils::FlagConfig("tegra_video") { reload(); }

    NX_FLAG(0, disable, "Use stub implementation.");

    NX_FLAG(0, enableOutput, "");
    NX_FLAG(0, enableTime, "");
    NX_FLAG(0, enableFps, "");

    NX_INT_PARAM(1, decodersCount, "Number of simultaneous hardware video decoders.");

    NX_INT_PARAM(1, tegraVideoCount, "Number of simultaneous TegraVideo instances.");

    NX_STRING_PARAM(
        //"../../data/model/GoogleNet-modified.prototxt",
        "/opt/networkoptix/mediaserver/bin/nvidia_models/GoogleNet-modified.prototxt",
        deployFile, "");

    NX_STRING_PARAM(
        //"../../data/model/GoogleNet-modified-online_iter_30000.caffemodel",
        "/opt/networkoptix/mediaserver/bin/nvidia_models/GoogleNet-modified-online_iter_30000.caffemodel",
        modelFile, "");

    NX_STRING_PARAM(
        //"./gieModel.cache",
        "/opt/networkoptix/mediaserver/var/cuda_engine.cache",
        cacheFile, "");

    NX_STRING_PARAM("", substituteFramesFilePrefix, "");

    NX_INT_PARAM(-1, cropRectX, "-1 means 0.");
    NX_INT_PARAM(-1, cropRectY, "-1 means 0.");
    NX_INT_PARAM(-1, cropRectW, "-1 means taking frame width.");
    NX_INT_PARAM(-1, cropRectH, "-1 means taking frame height.");
    NX_INT_PARAM(-1, maxInferenceFps, "-1 means unlimited. This value should be divided by 10");
};

extern TegraVideoFlagConfig conf;
