// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <QtCore/QStringList>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

constexpr auto kDeviceIssueEventGroup = "nx.events.group.deviceIssue";
constexpr auto kServerIssueEventGroup = "nx.events.group.serverIssue";

struct NX_VMS_RULES_API Group
{
    std::string id;
    QString name;

    QStringList items; //< Action or event ids.

    std::vector<Group> groups; //< Subgroups.

    const Group* findGroup(const std::string& id) const;
};

NX_VMS_RULES_API Group defaultEventGroups(common::SystemContext* context);

} // namespace nx::vms::rules
