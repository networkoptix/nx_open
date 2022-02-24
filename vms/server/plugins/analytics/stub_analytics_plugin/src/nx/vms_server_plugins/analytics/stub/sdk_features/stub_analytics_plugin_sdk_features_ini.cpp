// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stub_analytics_plugin_sdk_features_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace sdk_features {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace sdk_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
