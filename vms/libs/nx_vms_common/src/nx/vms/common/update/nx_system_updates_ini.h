// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::common::update {

struct NX_VMS_COMMON_API Ini: nx::kit::IniConfig
{
    Ini();

    NX_INI_STRING("", releaseListUrl,
        "If non-empty, overrides the update server URL. Use substring \"{customization}\" to\n"
        "substitute the current customization value, e.g.:\n"
        "`http://localhost/{customization}/releases.json`");
};

NX_VMS_COMMON_API Ini& ini();

} // namespace nx::vms::common::update
