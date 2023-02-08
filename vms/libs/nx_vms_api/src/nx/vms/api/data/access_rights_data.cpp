// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccessRightsData, (ubjson)(xml)(json)(sql_record)(csv_record), AccessRightsData_Fields)

const QnUuid kAllDevicesGroupId{"00000000-0000-0000-0000-200000000001"};
const QnUuid kAllWebPagesGroupId{"00000000-0000-0000-0000-200000000002"};
const QnUuid kAllServersGroupId{"00000000-0000-0000-0000-200000000003"};
const QnUuid kAllVideowallsGroupId{"00000000-0000-0000-0000-200000000004"};

const std::map<QnUuid, SpecialResourceGroup> kSpecialResourceGroupIds{
    {kAllDevicesGroupId, SpecialResourceGroup::allDevices},
    {kAllWebPagesGroupId, SpecialResourceGroup::allWebPages},
    {kAllServersGroupId, SpecialResourceGroup::allServers},
    {kAllVideowallsGroupId, SpecialResourceGroup::allVideowalls}};

std::optional<SpecialResourceGroup> specialResourceGroup(const QnUuid& id)
{
    if (const auto it = kSpecialResourceGroupIds.find(id); it != kSpecialResourceGroupIds.cend())
        return it->second;

    return std::nullopt;
}

QnUuid specialResourceGroupId(SpecialResourceGroup group)
{
    static const std::map<SpecialResourceGroup, QnUuid> reverseLookup =
        []()
        {
            std::map<SpecialResourceGroup, QnUuid> result;
            for (const auto& [groupId, group]: kSpecialResourceGroupIds)
                result.emplace(group, groupId);
            return result;
        }();

    const auto it = reverseLookup.find(group);
    return NX_ASSERT(it != reverseLookup.end()) ? it->second : QnUuid();
}

std::map<QnUuid, AccessRights> migrateAccessRights(
    GlobalPermissions permissions, const std::vector<QnUuid>& accessibleResources)
{
    if (permissions.testFlag(GlobalPermission::admin))
    {
        static const std::map<QnUuid, AccessRights> kAdminAccess{
            {kAllDevicesGroupId, kFullAccessRights},
            {kAllServersGroupId, kViewAccessRights},
            {kAllWebPagesGroupId, kViewAccessRights},
            {kAllVideowallsGroupId, kViewAccessRights}};

        return kAdminAccess;
    }

    AccessRights accessRights{};
    if (permissions.testFlag(GlobalPermission::editCameras))
        accessRights |= AccessRight::edit;
    if (permissions.testFlag(GlobalPermission::viewArchive))
        accessRights |= AccessRight::viewArchive;
    if (permissions.testFlag(GlobalPermission::exportArchive))
        accessRights |= AccessRight::exportArchive;
    if (permissions.testFlag(GlobalPermission::viewBookmarks))
        accessRights |= AccessRight::viewBookmarks;
    if (permissions.testFlag(GlobalPermission::manageBookmarks))
        accessRights |= AccessRight::manageBookmarks;
    if (permissions.testFlag(GlobalPermission::userInput))
        accessRights |= AccessRight::userInput;

    const auto fullAccessRights = accessRights | AccessRight::view;

    std::map<QnUuid, AccessRights> accessMap;
    if (permissions.testFlag(GlobalPermission::accessAllMedia))
    {
        accessMap.emplace(kAllDevicesGroupId, fullAccessRights);
        accessMap.emplace(kAllServersGroupId, AccessRight::view);
        accessMap.emplace(kAllWebPagesGroupId, AccessRight::view);
    }
    else if (accessRights)
    {
        accessMap.emplace(kAllDevicesGroupId, accessRights);
    }

    if (permissions.testFlag(GlobalPermission::controlVideowall))
        accessMap.emplace(kAllVideowallsGroupId, fullAccessRights);

    for (const auto& id: accessibleResources)
        accessMap.emplace(id, fullAccessRights);

    return accessMap;
}

} // namespace nx::vms::api
