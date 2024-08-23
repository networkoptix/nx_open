// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms_server_plugins::cloud_storage::stub {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_cloud_storage_plugin.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");
    NX_INI_STRING(".", workDir, "Working directory");
};

Ini& ini();

} // namespace nx::vms_server_plugins::cloud_storage::stub
