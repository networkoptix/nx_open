// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_types.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/to_string.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(PermissionsModel, PermissionsModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(PermissionsModel, (json))

const nx::Uuid kAllDevicesGroupId{"00000000-0000-0000-0000-200000000001"};
const nx::Uuid kAllWebPagesGroupId{"00000000-0000-0000-0000-200000000002"};
const nx::Uuid kAllServersGroupId{"00000000-0000-0000-0000-200000000003"};
const nx::Uuid kAllVideoWallsGroupId{"00000000-0000-0000-0000-200000000004"};

const std::map<nx::Uuid, SpecialResourceGroup> kSpecialResourceGroupIds{
    {kAllDevicesGroupId, SpecialResourceGroup::allDevices},
    {kAllWebPagesGroupId, SpecialResourceGroup::allWebPages},
    {kAllServersGroupId, SpecialResourceGroup::allServers},
    {kAllVideoWallsGroupId, SpecialResourceGroup::allVideowalls}};

std::optional<SpecialResourceGroup> specialResourceGroup(const nx::Uuid& id)
{
    if (const auto it = kSpecialResourceGroupIds.find(id); it != kSpecialResourceGroupIds.cend())
        return it->second;

    return std::nullopt;
}

nx::Uuid specialResourceGroupId(SpecialResourceGroup group)
{
    static const std::map<SpecialResourceGroup, nx::Uuid> reverseLookup = []()
    {
        std::map<SpecialResourceGroup, nx::Uuid> result;
        for (const auto& [groupId, group]: kSpecialResourceGroupIds)
            result.emplace(group, groupId);
        return result;
    }();

    const auto it = reverseLookup.find(group);
    return NX_ASSERT(it != reverseLookup.end()) ? it->second : nx::Uuid();
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
