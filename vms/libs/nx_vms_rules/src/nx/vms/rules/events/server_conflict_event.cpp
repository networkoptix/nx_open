// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_conflict_event.h"

namespace nx::vms::rules {

FilterManifest ServerConflictEvent::filterManifest()
{
    return {};
}

const ItemDescriptor& ServerConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.serverConflict",
        .displayName = tr("Server Conflict"),
        .description = "",
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
