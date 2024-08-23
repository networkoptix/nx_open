// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms_server_plugins::cloud_storage::stub
