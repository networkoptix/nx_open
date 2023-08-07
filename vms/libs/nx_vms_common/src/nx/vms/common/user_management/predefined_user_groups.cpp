// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "predefined_user_groups.h"

#include <unordered_map>

using namespace nx::core::access;
using namespace nx::vms::api;

namespace nx::vms::common {

// ------------------------------------------------------------------------------------------------
// PredefinedUserGroups::Private

struct PredefinedUserGroups::Private
{
    static QString name(const QnUuid& groupId)
    {
        if (groupId == kAdministratorsGroupId)
            return tr("Administrators");

        if (groupId == kPowerUsersGroupId)
            return tr("Power Users");

        if (groupId == kAdvancedViewersGroupId)
            return tr("Advanced Viewers");

        if (groupId == kViewersGroupId)
            return tr("Viewers");

        if (groupId == kLiveViewersGroupId)
            return tr("Live Viewers");

        if (groupId == kSystemHealthViewersGroupId)
            return tr("System Health Viewers");

        NX_ASSERT(false, "Not a predefined user group: %1", groupId);
        return {};
    }

    static QString description(const QnUuid& groupId)
    {
        if (groupId == kAdministratorsGroupId)
            return tr("This user has unlimited System privileges and cannot be deleted."
                " Can create and modify Administrators, and can merge Systems and link or unlink"
                " to Nx Cloud accounts.");

        if (groupId == kPowerUsersGroupId)
            return tr("Has full control of System configuration, but cannot create or modify"
                " other Power Users.");

        if (groupId == kAdvancedViewersGroupId)
            return tr("Can see and run PTZ positions and PTZ Tours, use 2-way audio, operate"
                " I/O Module buttons, create and edit Bookmarks, and view the Event Log.");

        if (groupId == kViewersGroupId)
            return tr("Can view and export archive and Bookmarks.");

        if (groupId == kLiveViewersGroupId)
            return tr("Can view live videos, I/O modules and web pages.");

        if (groupId == kSystemHealthViewersGroupId)
            return tr("Can view System Health Monitoring information.");

        NX_ASSERT(false, "Not a predefined user group: %1", groupId);
        return {};
    }

    static GlobalPermissions globalPermissions(const QnUuid& groupId)
    {
        if (groupId == kAdministratorsGroupId)
        {
            return GlobalPermission::administrator
                | GlobalPermission::powerUser
                | GlobalPermission::viewLogs
                | GlobalPermission::generateEvents
                | GlobalPermission::systemHealth;
        }

        if (groupId == kPowerUsersGroupId)
        {
            return GlobalPermission::powerUser
                | GlobalPermission::viewLogs
                | GlobalPermission::generateEvents
                | GlobalPermission::systemHealth;
        }

        if (groupId == kAdvancedViewersGroupId)
            return GlobalPermission::viewLogs;

        if (groupId == kSystemHealthViewersGroupId)
            return GlobalPermission::systemHealth;

        if (groupId == kViewersGroupId || groupId == kLiveViewersGroupId)
            return {};

        NX_ASSERT(false, "Not a predefined user group: %1", groupId);
        return {};
    };

    static const std::map<QnUuid, AccessRights>& accessRights(const QnUuid& groupId)
    {
        static constexpr AccessRights kViewerAccessRights = AccessRight::view
            | AccessRight::viewArchive
            | AccessRight::exportArchive
            | AccessRight::viewBookmarks;

        static constexpr AccessRights kAdvancedViewerAccessRights = kViewerAccessRights
            | AccessRight::manageBookmarks
            | AccessRight::userInput;

        if (groupId == kAdministratorsGroupId || groupId == kPowerUsersGroupId)
        {
            static const std::map<QnUuid, AccessRights> kPowerUserResourceAccessMap{
                {kAllDevicesGroupId, kFullAccessRights},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view},
                {kAllVideoWallsGroupId, AccessRight::edit}};

            return kPowerUserResourceAccessMap;
        }

        if (groupId == kAdvancedViewersGroupId)
        {
            static const std::map<QnUuid, AccessRights> kAdvancedViewerResourceAccessMap{
                {kAllDevicesGroupId, kAdvancedViewerAccessRights},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view}};

            return kAdvancedViewerResourceAccessMap;
        }

        if (groupId == kViewersGroupId)
        {
            static const std::map<QnUuid, AccessRights> kViewerResourceAccessMap{
                {kAllDevicesGroupId, kViewerAccessRights},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view}};

            return kViewerResourceAccessMap;
        }

        if (groupId == kLiveViewersGroupId)
        {
            static const std::map<QnUuid, AccessRights> kLiveViewerResourceAccessMap{
                {kAllDevicesGroupId, AccessRight::view},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view}};

            return kLiveViewerResourceAccessMap;
        }

        if (groupId == kSystemHealthViewersGroupId)
        {
            static const std::map<QnUuid, AccessRights> kSystemHealthViewerResourceAccessMap{
                {kAllServersGroupId, AccessRight::view}};

            return kSystemHealthViewerResourceAccessMap;
        }

        NX_ASSERT(false, "Not a predefined user group: %1", groupId);
        static const std::map<QnUuid, AccessRights> kEmpty;
        return kEmpty;
    }

    static const std::unordered_map<QnUuid, UserGroupData>& dataById()
    {
        static const auto kDataById =
            []()
            {
                std::unordered_map<QnUuid, UserGroupData> result;
                for (const auto groupId: PredefinedUserGroups::ids())
                {
                    UserGroupData group(groupId, name(groupId), globalPermissions(groupId));
                    group.attributes = UserAttribute::readonly;
                    group.description = description(groupId);
                    group.resourceAccessRights = accessRights(groupId);
                    result.emplace(groupId, std::move(group));
                }

                return result;
            }();

        return kDataById;
    }
};

// ------------------------------------------------------------------------------------------------
// PredefinedUserGroups

bool PredefinedUserGroups::contains(const QnUuid& groupId)
{
    return Private::dataById().contains(groupId);
}

std::optional<UserGroupData> PredefinedUserGroups::find(const QnUuid& predefinedGroupId)
{
    if (const auto it = Private::dataById().find(predefinedGroupId);
        it != Private::dataById().end())
    {
        return it->second;
    }

    return std::nullopt;
}

const UserGroupDataList& PredefinedUserGroups::list()
{
    static const UserGroupDataList kPredefinedUserGroupList =
        []()
        {
            UserGroupDataList result;
            for (const auto& groupId: PredefinedUserGroups::ids())
                result.push_back(*find(groupId));
            return result;
        }();

    return kPredefinedUserGroupList;
}

const QList<QnUuid>& PredefinedUserGroups::ids()
{
    static const QList<QnUuid> kIds =
        []()
        {
            QList<QnUuid> result{kPredefinedGroupIds.cbegin(), kPredefinedGroupIds.cend()};
            std::sort(result.begin(), result.end());
            return result;
        }();

    return kIds;
}

} // namespace nx::vms::common
