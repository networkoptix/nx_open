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
            return tr("This user has unlimited System privileges."
                " Can merge Systems and link or unlink to Nx Cloud accounts.");

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

    static ResourceAccessMap accessRights(const QnUuid& groupId)
    {
        static constexpr AccessRights kViewerAccessRights = AccessRight::view
            | AccessRight::viewArchive
            | AccessRight::exportArchive
            | AccessRight::viewBookmarks;

        static constexpr AccessRights kAdvancedViewerAccessRights = kViewerAccessRights
            | AccessRight::manageBookmarks
            | AccessRight::userInput;

        if (groupId == kAdministratorsGroupId || groupId == kPowerUsersGroupId)
            return kFullResourceAccessMap;

        if (groupId == kAdvancedViewersGroupId)
        {
            static const ResourceAccessMap kAdvancedViewerResourceAccessMap{
                {kAllDevicesGroupId, kAdvancedViewerAccessRights},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view}};

            return kAdvancedViewerResourceAccessMap;
        }

        if (groupId == kViewersGroupId)
        {
            static const ResourceAccessMap kViewerResourceAccessMap{
                {kAllDevicesGroupId, kViewerAccessRights},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view}};

            return kViewerResourceAccessMap;
        }

        if (groupId == kLiveViewersGroupId)
        {
            static const ResourceAccessMap kLiveViewerResourceAccessMap{
                {kAllDevicesGroupId, AccessRight::view},
                {kAllWebPagesGroupId, AccessRight::view},
                {kAllServersGroupId, AccessRight::view}};

            return kLiveViewerResourceAccessMap;
        }

        if (groupId == kSystemHealthViewersGroupId)
        {
            static const ResourceAccessMap kSystemHealthViewerResourceAccessMap{
                {kAllServersGroupId, AccessRight::view}};

            return kSystemHealthViewerResourceAccessMap;
        }

        NX_ASSERT(false, "Not a predefined user group: %1", groupId);
        return {};
    }

    static const std::unordered_map<QnUuid, UserGroupData>& dataById()
    {
        static const auto kDataById =
            []()
            {
                std::unordered_map<QnUuid, UserGroupData> result;
                for (const auto groupId: PredefinedUserGroups::ids())
                {
                    result.emplace(groupId, UserGroupData::makePredefined(
                        groupId,
                        name(groupId),
                        description(groupId),
                        globalPermissions(groupId)));
                }

                return result;
            }();

        return kDataById;
    }

    static const std::unordered_map<QnUuid, ResourceAccessMap>& accessRightsById()
    {
        static const auto kAccessRightsById =
            []()
            {
                std::unordered_map<QnUuid, ResourceAccessMap> result;
                for (const auto& groupId: PredefinedUserGroups::ids())
                    result.emplace(groupId, accessRights(groupId));
                return result;
            }();

        return kAccessRightsById;
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

ResourceAccessMap PredefinedUserGroups::accessRights(const QnUuid& groupId)
{
    if (const auto it = Private::accessRightsById().find(groupId);
        it != Private::accessRightsById().end())
    {
        return it->second;
    }

    return {};
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
