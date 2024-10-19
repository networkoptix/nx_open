// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_interface_watcher.h"

#include <core/resource_management/resource_discovery_manager.h>
#include <mobile_client/mobile_client_module.h>

namespace nx::vms::client::mobile {

NetworkInterfaceWatcher::NetworkInterfaceWatcher()
{
    connect(
        qnMobileClientModule->resourceDiscoveryManager(),
        &QnResourceDiscoveryManager::localInterfacesChanged,
        this,
        &NetworkInterfaceWatcher::interfacesChanged);
}

} // namespace nx::vms::client::mobile
