// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::build_info {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_build_info.ini") { reload(); }

    NX_INI_STRING("", publicationType,
        "Override the publication type.\n"
        "Possible values: local, private_build, private_patch, patch, beta, rc, release.");
};

NX_BUILD_INFO_API Ini& ini();

} // namespace nx::build_info
