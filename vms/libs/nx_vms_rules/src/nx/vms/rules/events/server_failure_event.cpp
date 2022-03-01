// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

namespace nx::vms::rules {

FilterManifest ServerFailureEvent::filterManifest()
{
    return {};
}

const ItemDescriptor& ServerFailureEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.serverFailure",
        .displayName = tr("Server Failure"),
        .description = "",
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
