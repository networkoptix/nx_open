// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

struct NxImageIniConfig: public nx::kit::IniConfig
{
    NxImageIniConfig(): nx::kit::IniConfig("nx_image.ini") { reload(); }

    NX_INI_FLAG(1, enableSseColorConvertion,
        "Enables SSE color convertion optimizations. If false Ffmpeg sws_scale will be used");
};

NX_MEDIA_CORE_API NxImageIniConfig& nxImageIni();
