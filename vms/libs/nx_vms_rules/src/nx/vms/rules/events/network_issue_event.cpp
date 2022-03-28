// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_event.h"

namespace nx::vms::rules {

const ItemDescriptor& NetworkIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.networkIssue",
        .displayName = tr("Network Issue"),
        .description = "",
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
