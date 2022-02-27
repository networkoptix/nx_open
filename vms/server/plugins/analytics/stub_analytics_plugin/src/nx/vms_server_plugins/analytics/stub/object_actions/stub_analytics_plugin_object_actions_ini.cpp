// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stub_analytics_plugin_object_actions_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_actions {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace object_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
