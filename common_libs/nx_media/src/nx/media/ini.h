#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace media {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_media.ini") {}

    NX_INI_STRING("", substitutePlayerUrl, "Use this Url for video, e.g. file:///c:/test.MP4");
    NX_INI_FLAG(0, outputFrameDelays, "Log if frame delay is negative.");
    NX_INI_FLAG(0, enableFpsPresentNextFrame, "Measure FPS at presentNextFrame() start.");
    NX_INI_INT(-1, hwVideoX, "If not -1, override hardware video window X.");
    NX_INI_INT(-1, hwVideoY, "If not -1, override hardware video window Y.");
    NX_INI_INT(-1, hwVideoWidth, "If not -1, override hardware video window width.");
    NX_INI_INT(-1, hwVideoHeight, "If not -1, override hardware video window height.");
    NX_INI_INT(2000, allowedAnalyticsMetadataDelay,
        "Allowed delay to display old analytics metadata before new is received (milliseconds).");
    NX_INI_FLAG(0, forceIframesOnly, "For Low Quality selection, force I-frames-only mode.");
    NX_INI_FLAG(0, unlimitFfmpegMaxResolution, "");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace media
} // namespace nx
