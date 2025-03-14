// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stub_analytics_integration_diagnostic_events_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace diagnostic_events {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace diagnostic_events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
