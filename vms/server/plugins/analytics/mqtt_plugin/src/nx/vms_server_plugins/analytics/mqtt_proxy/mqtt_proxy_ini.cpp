// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_proxy_ini.h"

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // nx::vms_server_plugins::analytics::mqtt_proxy
