// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "predefined_user_groups.h"

#include <unordered_map>

#include <nx/branding.h>

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
            return tr("Members of this group have unlimited System privileges. Administrators can "
                "create and modify Power Users, merge Systems and connect or disconnect "
                "System to  %1.", "%1 is the cloud name (like Nx Cloud)")
                .arg(nx::branding::cloudName());

        if (groupId == kPowerUsersGroupId)
            return tr("Members of this group can, in addition to the permissions granted by the "
                "Advanced Viewers group, control most of the System configuration, but are not "
                "allowed to change any Administrator related settings, like delete or change "
                "their own groups and permissions, and cannot create or edit other Power Users.");

        if (groupId == kAdvancedViewersGroupId)
            return tr("Members of this group can, in addition to the permissions granted by the "
                "Viewers group, see and activate PTZ positions and PTZ tours, use 2-way audio, "
                "operate I/O module buttons, create and edit bookmarks, and view the Event Log.");

        if (groupId == kViewersGroupId)
            return tr("Members of this group can, in addition to the permissions granted by the "
                "Live Viewers group, view and export archive and Bookmarks.");

        if (groupId == kLiveViewersGroupId)
            return tr("Members of this group can view live videos, I/O modules and web pages.");

        if (groupId == kSystemHealthViewersGroupId)
            return tr("Members of this group can view System Health Monitoring information and "
                "server processor load in real-time (Server Monitoring).");

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
                | GlobalPermission::viewMetrics;
        }

        if (groupId == kPowerUsersGroupId)
        {
            return GlobalPermission::powerUser
                | GlobalPermission::viewLogs
                | GlobalPermission::generateEvents
                | GlobalPermission::viewMetrics;
        }

        if (groupId == kAdvancedViewersGroupId)
            return GlobalPermission::viewLogs;

        if (groupId == kSystemHealthViewersGroupId)
            return GlobalPermission::viewMetrics;

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
