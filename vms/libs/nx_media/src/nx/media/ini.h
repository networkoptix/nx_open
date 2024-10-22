// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace media {

struct NX_MEDIA_API Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_media.ini") { reload(); }

    NX_INI_FLAG(0, outputFrameDelays, "Log if frame delay is negative.");
    NX_INI_FLAG(0, enableFpsPresentNextFrame, "Measure FPS at presentNextFrame() start.");
    NX_INI_INT(-1, hwVideoX, "If not -1, override hardware video window X.");
    NX_INI_INT(-1, hwVideoY, "If not -1, override hardware video window Y.");
    NX_INI_INT(-1, hwVideoWidth, "If not -1, override hardware video window width.");
    NX_INI_INT(-1, hwVideoHeight, "If not -1, override hardware video window height.");
    NX_INI_INT(128 * 1024 * 1024, nvidiaFreeMemoryLimit,
        "Nvidia GPU memory, that should be free when trying to open a new hardware decoder.");
    NX_INI_INT(2000, allowedAnalyticsMetadataDelayMs,
        "Allowed delay to display old analytics metadata before new is received.");
    NX_INI_INT(1000, metadataCacheSize, "Size of metadata cache per channel.");
    NX_INI_FLAG(0, forceIframesOnly, "For Low Quality selection, force I-frames-only mode.");
    NX_INI_FLAG(0, unlimitFfmpegMaxResolution, "");
    NX_INI_FLAG(0, allowSpeedupAudio, "Allow fast audio playing during x2 and x4 speed");
};

NX_MEDIA_API Ini& ini();

} // namespace media
} // namespace nx
