// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group.h"

#include <QCoreApplication>

namespace nx::vms::rules {

namespace {

class GroupStrings
{
    Q_DECLARE_TR_FUNCTIONS(GroupStrings)
};

} // namespace

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

Group defaultEventGroups()
{
    return Group{
        .id = {},
        .name = GroupStrings::tr("Any event"),
        .groups = {
            {
                .id = kDeviceIssueEventGroup,
                .name = GroupStrings::tr("Any camera issue"),
            },
            {
                .id = kServerIssueEventGroup,
                .name = GroupStrings::tr("Any server issue"),
            },

        },
    };
}

} // namespace nx::vms::rules
