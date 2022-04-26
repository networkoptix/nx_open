// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_issue_event.h"

namespace nx::vms::rules {

QString LicenseIssueEvent::uniqueName() const
{
    // Licence issue event does not require uniqness offered by the ReasonedEvent::uniqueName().
    return BasicEvent::uniqueName();
}

const ItemDescriptor& LicenseIssueEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.licenseIssue",
        .displayName = tr("License Issue"),
        .description = "",
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
