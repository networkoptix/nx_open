// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::desktop::integrations {

struct Config: nx::kit::IniConfig
{
    Config(): IniConfig("desktop_client_integrations.ini") { reload(); }
};

Config& config();

} // namespace nx::vms::client::desktop::integrations
