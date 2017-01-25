#pragma once

#include <nx/utils/flag_config.h>

namespace nx {
namespace media {

struct FlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_STRING_PARAM("", substitutePlayerUrl, "Use this Url for video, e.g. file:///c:/test.MP4");
    NX_FLAG(0, outputFrameDelays, "Log if frame delay is negative.");
    NX_FLAG(0, enableFps, "");
    NX_INT_PARAM(-1, hwVideoX, "If not -1, override hardware video window X.");
    NX_INT_PARAM(-1, hwVideoY, "If not -1, override hardware video window Y.");
    NX_INT_PARAM(-1, hwVideoWidth, "If not -1, override hardware video window width.");
    NX_INT_PARAM(-1, hwVideoHeight, "If not -1, override hardware video window height.");
    NX_FLAG(0, forceIframesOnly, "For Low Quality selection, force I-frames-only mode.");
    NX_FLAG(0, unlimitFfmpegMaxResolution, "");
};
extern FlagConfig conf;

} // namespace media
} // namespace nx
