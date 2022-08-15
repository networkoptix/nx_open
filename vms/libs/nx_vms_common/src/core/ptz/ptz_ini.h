// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

struct PtzIni: public nx::kit::IniConfig
{
    PtzIni(): IniConfig("ptz.ini") { reload(); }

    NX_INI_INT(32, maxPtzControllerPoolThreadCount,
        "The maximum number of threads in the Ptz Controller Pool. Up to 5.0, for ARM32 devices\n"
        "it used to be 8.");
};

NX_VMS_COMMON_API PtzIni& ptzIni();
