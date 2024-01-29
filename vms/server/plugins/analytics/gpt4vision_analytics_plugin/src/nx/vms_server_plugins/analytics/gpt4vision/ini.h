// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms_server_plugins::analytics::gpt4vision {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("gpt4vision_analytics_plugin.ini") { reload(); }

    NX_INI_FLAG(false, enableOutput, "");
};

Ini& ini();

} // namespace nx::vms_server_plugins::analytics::gpt4vision
