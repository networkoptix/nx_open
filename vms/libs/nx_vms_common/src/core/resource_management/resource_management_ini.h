// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

struct ResourceManagementIni: public nx::kit::IniConfig
{
    ResourceManagementIni(): IniConfig("resource_management.ini") { reload(); }

    NX_INI_FLAG(false, allowDefaultStorageFactory,
        "Allows using the default storage factory for external storage creation.");
    NX_INI_INT(15'000, cameraDiscoveryIntervalMs,
        "Wait at least this many milliseconds between the camera discovery attempts.");
};

NX_VMS_COMMON_API ResourceManagementIni& resourceManagementIni();
