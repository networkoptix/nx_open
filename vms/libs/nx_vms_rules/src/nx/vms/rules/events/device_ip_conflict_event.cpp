// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ip_conflict_event.h"

namespace nx::vms::rules {

FilterManifest DeviceIpConflictEvent::filterManifest()
{
    return {};
}

const ItemDescriptor& DeviceIpConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.deviceIpConflict",
        .displayName = tr("Device IP Conflict"),
        .description = "",
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
