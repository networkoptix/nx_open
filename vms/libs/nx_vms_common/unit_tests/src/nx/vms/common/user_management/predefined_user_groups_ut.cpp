// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <core/resource_access/resource_access_map.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>

using namespace nx::core::access;
using namespace nx::vms::api;

namespace nx::vms::common {
namespace test {

TEST(PredefinedUserGroups, contains)
{
    for (const auto& id: kPredefinedGroupIds)
        EXPECT_TRUE(PredefinedUserGroups::contains(id));

    EXPECT_FALSE(PredefinedUserGroups::contains(QnUuid{}));
    EXPECT_FALSE(PredefinedUserGroups::contains(QnUuid::createUuid()));
}

TEST(PredefinedUserGroups, ids)
{
    const auto ids = PredefinedUserGroups::ids();
    EXPECT_EQ(ids.size(), kPredefinedGroupIds.size());
    EXPECT_EQ(std::set<QnUuid>(ids.begin(), ids.end()), kPredefinedGroupIds);
}

TEST(PredefinedUserGroups, list)
{
    const auto list = PredefinedUserGroups::list();
    EXPECT_EQ(list.size(), kPredefinedGroupIds.size());

    for (const auto& group: list)
    {
        EXPECT_TRUE(kPredefinedGroupIds.contains(group.id));
        EXPECT_TRUE(group.attributes.testFlag(nx::vms::api::UserAttribute::readonly));
        EXPECT_TRUE(group.parentGroupIds.empty());
        EXPECT_FALSE(group.name.isEmpty());
        EXPECT_FALSE(group.description.isEmpty());
        EXPECT_NE(group.type, UserType::ldap);
    }
}

TEST(PredefinedUserGroups, find)
{
    for (const auto& group: PredefinedUserGroups::list())
    {
        const auto found = PredefinedUserGroups::find(group.id);
        ASSERT_TRUE((bool) found);
        EXPECT_EQ(group, *found);
    }

    EXPECT_FALSE((bool) PredefinedUserGroups::find(QnUuid{}));
    EXPECT_FALSE((bool) PredefinedUserGroups::find(QnUuid::createUuid()));
}

TEST(PredefinedUserGroups, globalPermissions)
{
    EXPECT_EQ(PredefinedUserGroups::find(kAdministratorsGroupId)->permissions,
        GlobalPermission::administrator
        | GlobalPermission::powerUser
        | GlobalPermission::viewLogs
        | GlobalPermission::generateEvents
        | GlobalPermission::viewMetrics);

    EXPECT_EQ(PredefinedUserGroups::find(kPowerUsersGroupId)->permissions,
        GlobalPermission::powerUser
        | GlobalPermission::viewLogs
        | GlobalPermission::generateEvents
        | GlobalPermission::viewMetrics);

    EXPECT_EQ(PredefinedUserGroups::find(kAdvancedViewersGroupId)->permissions,
        GlobalPermission::viewLogs);

    EXPECT_EQ(PredefinedUserGroups::find(kViewersGroupId)->permissions,
        GlobalPermission::none);

    EXPECT_EQ(PredefinedUserGroups::find(kLiveViewersGroupId)->permissions,
        GlobalPermission::none);

    EXPECT_EQ(PredefinedUserGroups::find(kSystemHealthViewersGroupId)->permissions,
        GlobalPermission::viewMetrics);
}

TEST(PredefinedUserGroups, accessRights)
{
    static const std::map<QnUuid, AccessRights> kPowerUserResourceAccessMap{
        {kAllDevicesGroupId, kFullAccessRights},
        {kAllWebPagesGroupId, AccessRight::view},
        {kAllServersGroupId, AccessRight::view},
        {kAllVideoWallsGroupId, AccessRight::edit}};

    static const std::map<QnUuid, AccessRights> kAdvancedViewersResourceAccessMap{
        {kAllDevicesGroupId, AccessRight::view
            | AccessRight::viewArchive
            | AccessRight::exportArchive
            | AccessRight::viewBookmarks
            | AccessRight::manageBookmarks
            | AccessRight::userInput},
        {kAllWebPagesGroupId, AccessRight::view},
        {kAllServersGroupId, AccessRight::view}};

    static const std::map<QnUuid, AccessRights> kViewersResourceAccessMap{
        {kAllDevicesGroupId, AccessRight::view
            | AccessRight::viewArchive
            | AccessRight::exportArchive
            | AccessRight::viewBookmarks},
        {kAllWebPagesGroupId, AccessRight::view},
        {kAllServersGroupId, AccessRight::view}};

    static const std::map<QnUuid, AccessRights> kLiveViewersResourceAccessMap{
        {kAllDevicesGroupId, AccessRight::view},
        {kAllWebPagesGroupId, AccessRight::view},
        {kAllServersGroupId, AccessRight::view}};

    static const std::map<QnUuid, AccessRights> kHealthViewersResourceAccessMap{
        {kAllServersGroupId, AccessRight::view}};

    EXPECT_EQ(PredefinedUserGroups::find(kAdministratorsGroupId)->resourceAccessRights,
        kPowerUserResourceAccessMap);

    EXPECT_EQ(PredefinedUserGroups::find(kPowerUsersGroupId)->resourceAccessRights,
        kPowerUserResourceAccessMap);

    EXPECT_EQ(PredefinedUserGroups::find(kAdvancedViewersGroupId)->resourceAccessRights,
        kAdvancedViewersResourceAccessMap);

    EXPECT_EQ(PredefinedUserGroups::find(kViewersGroupId)->resourceAccessRights,
        kViewersResourceAccessMap);

    EXPECT_EQ(PredefinedUserGroups::find(kLiveViewersGroupId)->resourceAccessRights,
        kLiveViewersResourceAccessMap);

    EXPECT_EQ(PredefinedUserGroups::find(kSystemHealthViewersGroupId)->resourceAccessRights,
        kHealthViewersResourceAccessMap);
}

} // namespace test
} // namespace nx::vms::common
