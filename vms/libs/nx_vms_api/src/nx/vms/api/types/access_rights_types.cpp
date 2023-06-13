// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_types.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/to_string.h>

namespace nx::vms::api {

const QnUuid kAllDevicesGroupId{"00000000-0000-0000-0000-200000000001"};
const QnUuid kAllWebPagesGroupId{"00000000-0000-0000-0000-200000000002"};
const QnUuid kAllServersGroupId{"00000000-0000-0000-0000-200000000003"};
const QnUuid kAllVideoWallsGroupId{"00000000-0000-0000-0000-200000000004"};

const std::map<QnUuid, SpecialResourceGroup> kSpecialResourceGroupIds{
    {kAllDevicesGroupId, SpecialResourceGroup::allDevices},
    {kAllWebPagesGroupId, SpecialResourceGroup::allWebPages},
    {kAllServersGroupId, SpecialResourceGroup::allServers},
    {kAllVideoWallsGroupId, SpecialResourceGroup::allVideowalls}};

std::optional<SpecialResourceGroup> specialResourceGroup(const QnUuid& id)
{
    if (const auto it = kSpecialResourceGroupIds.find(id); it != kSpecialResourceGroupIds.cend())
        return it->second;

    return std::nullopt;
}

QnUuid specialResourceGroupId(SpecialResourceGroup group)
{
    static const std::map<SpecialResourceGroup, QnUuid> reverseLookup = []()
    {
        std::map<SpecialResourceGroup, QnUuid> result;
        for (const auto& [groupId, group]: kSpecialResourceGroupIds)
            result.emplace(group, groupId);
        return result;
    }();

    const auto it = reverseLookup.find(group);
    return NX_ASSERT(it != reverseLookup.end()) ? it->second : QnUuid();
}

void PrintTo(GlobalPermission value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(GlobalPermissions value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(AccessRight value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(AccessRights value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(SpecialResourceGroup value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

} // namespace nx::vms::api
