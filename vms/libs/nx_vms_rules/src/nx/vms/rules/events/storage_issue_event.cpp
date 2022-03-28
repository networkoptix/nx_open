// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_issue_event.h"

namespace nx::vms::rules {

const ItemDescriptor& StorageIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.storageIssue",
        .displayName = tr("Storage Issue"),
        .description = "",
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
