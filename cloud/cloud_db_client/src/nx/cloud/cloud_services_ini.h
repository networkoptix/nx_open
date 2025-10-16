// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::cloud {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("cloud_services.ini") { reload(); }

    NX_INI_STRING("", customizedAnalyticsDbUrl,
        "Overrides the current analytycs DB service url");

    NX_INI_STRING("", customizedDeploymentServiceUrl,
        "Overrides the current Deployment service url");

    NX_INI_STRING("", customizedMotionDbServiceUrl,
        "Overrides the current Deployment service url");

};

Ini& ini();

} // nx::cloud
