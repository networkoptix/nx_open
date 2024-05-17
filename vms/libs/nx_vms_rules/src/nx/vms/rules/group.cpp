// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group.h"

#include "strings.h"

namespace nx::vms::rules {

const Group* Group::findGroup(const std::string& id) const
{
    if (this->id == id)
        return this;

    for (const auto& group : groups)
    {
        if (group.findGroup(id))
            return &group;
    }

    return nullptr;
}

Group defaultEventGroups(common::SystemContext* context)
{
    return Group{
        .id = {},
        .name = Strings::anyEvent(),
        .groups = {
            {
                .id = kDeviceIssueEventGroup,
                .name = Strings::anyDeviceIssue(context),
            },
            {
                .id = kServerIssueEventGroup,
                .name = Strings::anyServerEvent(),
            },

        },
    };
}

} // namespace nx::vms::rules
