#pragma once

#include <nx/kit/ini_config.h>

/**
 * Used by libtegra_video.so library; also used by video_dec_gie executable.
 */
struct TegraVideoIniConfig: public nx::kit::IniConfig
{
    TegraVideoIniConfig(): nx::kit::IniConfig("tegra_video.ini") { reload(); }

    NX_INI_FLAG(0, disable, "Use stub implementation which does not use NVidia-specific hw.");

    NX_INI_FLAG(0, enableOutput, "");
    NX_INI_FLAG(0, enableTime, "");

    NX_INI_INT(1, decodersCount, "Number of simultaneous hardware video decoders.");
    NX_INI_INT(-1, cropRectX, "-1 means 0.");
    NX_INI_INT(-1, cropRectY, "-1 means 0.");
    NX_INI_INT(-1, cropRectW, "-1 means taking frame width.");
    NX_INI_INT(-1, cropRectH, "-1 means taking frame height.");
    NX_INI_INT(-1, maxInferenceFps, "-1 means unlimited. This value will be divided by 10.");
};

inline TegraVideoIniConfig& ini()
{
    static TegraVideoIniConfig ini;
    return ini;
}
