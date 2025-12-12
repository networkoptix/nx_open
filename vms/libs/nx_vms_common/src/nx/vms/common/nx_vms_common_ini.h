// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::common {

struct NX_VMS_COMMON_API Ini: nx::kit::IniConfig
{
    Ini();

    NX_INI_STRING("https://licensing.vmsproxy.com", defaultLicenseServerUrl,
        "License server URL (can be overridden in the system settings)");
};

NX_VMS_COMMON_API Ini& ini();

} // namespace nx::vms::common
