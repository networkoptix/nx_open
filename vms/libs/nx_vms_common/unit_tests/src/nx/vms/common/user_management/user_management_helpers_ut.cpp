// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

using namespace nx::vms::api;

namespace nx::vms::common {
namespace test {

class UserManagementHelpersTest: public ContextBasedTest
{
    virtual void SetUp() override
    {
        ContextBasedTest::SetUp();
        d.administrator = addUser(kAdministratorsGroupId, "administrator");
        d.powerUser = addUser(kPowerUsersGroupId, "power user");
        d.viewer = addUser(kViewersGroupId, "viewer");

        d.specialWorkers = createUserGroup("Special Workers");
        d.specialPowerUsers =
            createUserGroup("Special Power Users", {d.specialWorkers.id, kPowerUsersGroupId});
        d.specialViewers =
            createUserGroup("Special Viewers", {d.specialWorkers.id, kViewersGroupId});

        d.specialPowerUser = addUser(d.specialPowerUsers.id, "special power user");
        d.specialViewer = addUser(d.specialViewers.id, "special viewer");

        d.specialAdministrator = addUser({d.specialWorkers.id, kAdministratorsGroupId},
            "special administrator");
    }

    virtual void TearDown() override
    {
        d = {};
        ContextBasedTest::TearDown();
    }

protected:
    struct
    {
        QnUserResourcePtr administrator;
        QnUserResourcePtr powerUser;
        QnUserResourcePtr viewer;
        QnUserResourcePtr specialPowerUser;
        QnUserResourcePtr specialViewer;
        QnUserResourcePtr specialAdministrator;

        UserGroupData specialWorkers;
        UserGroupData specialPowerUsers;
        UserGroupData specialViewers;
    } d;
};

TEST_F(UserManagementHelpersTest, getUsersAndGroups)
{
    const std::vector<nx::Uuid> requestedIds{{
        nx::Uuid::createUuid(),
        nx::Uuid::createUuid(),
        d.powerUser->getId(),
        nx::Uuid{},
        nx::Uuid::createUuid(),
        d.specialPowerUsers.id,
        kPowerUsersGroupId,
        nx::Uuid::createUuid(),
        nx::Uuid::createUuid(),
        d.specialPowerUser->getId(),
        nx::Uuid{},
        nx::Uuid::createUuid()}};

    QnUserResourceList users;
    UserGroupDataList groups;
    getUsersAndGroups(systemContext(), requestedIds, users, groups);
    EXPECT_EQ(users, QnUserResourceList({d.powerUser, d.specialPowerUser}));
    EXPECT_EQ(groups, UserGroupDataList(
        {d.specialPowerUsers, *PredefinedUserGroups::find(kPowerUsersGroupId)}));

    QList<nx::Uuid> groupIds;
    getUsersAndGroups(systemContext(), requestedIds, users, groupIds);
    EXPECT_EQ(users, QnUserResourceList({d.powerUser, d.specialPowerUser}));
    EXPECT_EQ(groupIds, QList<nx::Uuid>({d.specialPowerUsers.id, kPowerUsersGroupId}));
}

TEST_F(UserManagementHelpersTest, allGroupsExist)
{
    EXPECT_TRUE(allUserGroupsExist(systemContext(), std::initializer_list<nx::Uuid>({
        kLiveViewersGroupId, kAdministratorsGroupId, d.specialPowerUsers.id, d.specialViewers.id})));

    EXPECT_FALSE(allUserGroupsExist(systemContext(), std::initializer_list<nx::Uuid>({
        d.specialPowerUsers.id, /*invalid*/ nx::Uuid{}})));

    EXPECT_FALSE(allUserGroupsExist(systemContext(), std::initializer_list<nx::Uuid>({
        d.specialPowerUsers.id, /*unknown*/ nx::Uuid::createUuid()})));

    EXPECT_FALSE(allUserGroupsExist(systemContext(), std::initializer_list<nx::Uuid>({
        d.specialPowerUsers.id, /*user*/ d.specialViewer->getId()})));
}

TEST_F(UserManagementHelpersTest, groupNames)
{
    EXPECT_EQ(userGroupNames(systemContext(), std::initializer_list<nx::Uuid>({
        d.specialWorkers.id, /*invalid*/ nx::Uuid{}, d.specialPowerUsers.id, d.specialViewers.id})),
        QStringList({d.specialWorkers.name, d.specialPowerUsers.name, d.specialViewers.name}));

    EXPECT_EQ(userGroupNames(d.specialAdministrator), QStringList(
        {d.specialWorkers.name, PredefinedUserGroups::find(kAdministratorsGroupId)->name}));
}

TEST_F(UserManagementHelpersTest, groupsWithParents)
{   /*
          Group 1         *Power Users*          Special Workers          *Viewers*
            |                    |                  |       |                  |
       +----+----+               +---------+--------+       +---------+--------+
       |         |                         |                          |
       v         v                         v                          v
     Group 2    Group 3          Special Power Users          Special Viewers
       |         |  |
       +----+----+  |
            |       |
            v       v
         Group 4  Group 5
    */

    const auto group1 = createUserGroup("Group 1");
    const auto group2 = createUserGroup("Group 2", group1.id);
    const auto group3 = createUserGroup("Group 3", group1.id);
    const auto group4 = createUserGroup("Group 4", {group2.id, group3.id});
    const auto group5 = createUserGroup("Group 5", group3.id);

    EXPECT_EQ(userGroupsWithParents(systemContext(),
        std::initializer_list<nx::Uuid>({group4.id, d.specialPowerUsers.id})),
        QSet<nx::Uuid>({group4.id, group3.id, group2.id, group1.id,
            d.specialPowerUsers.id, d.specialWorkers.id, kPowerUsersGroupId}));

    EXPECT_EQ(userGroupsWithParents(systemContext(),
        std::initializer_list<nx::Uuid>({group5.id, d.specialViewers.id})),
        QSet<nx::Uuid>({group5.id, group3.id, group1.id,
            d.specialViewers.id, d.specialWorkers.id, kViewersGroupId}));
}

} // namespace test
} // namespace nx::vms::common
