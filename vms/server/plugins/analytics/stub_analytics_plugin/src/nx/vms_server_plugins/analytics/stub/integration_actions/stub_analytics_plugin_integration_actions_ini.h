// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace integration_actions {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_analytics_plugin_integration_actions.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");
};

Ini& ini();

} // namespace integration_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
