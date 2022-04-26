// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_event.h"

namespace nx::vms::rules {

QString NetworkIssueEvent::uniqueName() const
{
    return makeName(
        ReasonedEvent::uniqueName(),
        serverId().toString(),
        QString::number(static_cast<int>(reasonCode())));
}

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
