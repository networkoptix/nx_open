#pragma once

#include <nx/utils/flag_config.h>

struct TegraVideoFlagConfig: public nx::utils::FlagConfig
{
    TegraVideoFlagConfig(): nx::utils::FlagConfig("tegra_video") { reload(); }

    NX_FLAG(0, disable, "Use stub implementation.");

    NX_FLAG(0, enableOutput, "");
    NX_FLAG(0, enableTime, "");
    NX_FLAG(0, enableFps, "");

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
};

extern TegraVideoFlagConfig conf;
