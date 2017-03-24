#pragma once

#include <nx/utils/flag_config.h>

struct TegraVideoFlagConfig: public nx::utils::FlagConfig
{
    TegraVideoFlagConfig(): nx::utils::FlagConfig("tegra_video") {}

    NX_FLAG(0, disable, "Use stub implementation.");

    NX_FLAG(0, enableOutput, "");
    NX_FLAG(0, enableTime, "");
    NX_FLAG(0, enableFps, "");
};

extern TegraVideoFlagConfig conf;
