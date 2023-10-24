// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_administration/watchers/non_editable_users_and_groups.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

namespace test {

class NonEditableUsersAndGroupsTest: public nx::vms::client::desktop::test::ContextBasedTest
{
public:
    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
    }

    void loginAs(const QnUuid& userId)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto user = resourcePool->getResourceById<QnUserResource>(userId);
        ASSERT_TRUE(!user.isNull());
        systemContext()->accessController()->setUser(user);

        QObject::connect(systemContext()->nonEditableUsersAndGroups(),
            &NonEditableUsersAndGroups::userModified,
            [this](const QnUserResourcePtr& user)
            {
                const auto action = systemContext()->nonEditableUsersAndGroups()->containsUser(user)
                    ? "added"
                    : "removed";

                signalLog << nx::format("%1 %2", action, user->getName());
            });

        QObject::connect(systemContext()->nonEditableUsersAndGroups(),
            &NonEditableUsersAndGroups::groupModified,
            [this](const QnUuid& groupId)
            {
                const auto action =
                    systemContext()->nonEditableUsersAndGroups()->containsGroup(groupId)
                        ? "added"
                        : "removed";

                const auto group = systemContext()->userGroupManager()->find(groupId);
                ASSERT_TRUE(group);

                signalLog << nx::format("%1 %2", action, group->name);
            });
    }

    QnUuid addUser(
        const QString& name,
        const std::vector<QnUuid>& parents,
        const api::UserType& userType = api::UserType::local)
    {
        QnUserResourcePtr user(
            new QnUserResource(userType, /*externalId*/ {}));
        user->setIdUnsafe(QnUuid::createUuid());
        user->setName(name);
        user->addFlags(Qn::remote);
        user->setGroupIds(parents);
        systemContext()->resourcePool()->addResource(user);
        return user->getId();
    }

    QnUuid addGroup(
        const QString& name,
        const std::vector<QnUuid>& parents,
        nx::vms::api::UserType type = nx::vms::api::UserType::local)
    {
        api::UserGroupData group;
        group.setId(QnUuid::createUuid());
        group.name = name;
        group.type = type;
        group.parentGroupIds = parents;
        systemContext()->userGroupManager()->addOrUpdate(group);
        return group.id;
    }

    void removeUser(const QnUuid& id)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto resource = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!resource.isNull());
        resourcePool->removeResource(resource);
    }

    void removeGroup(const QnUuid& id)
    {
        const auto allUsers = systemContext()->resourcePool()->getResources<QnUserResource>();
        for (const auto& user: allUsers)
        {
            std::vector<QnUuid> ids = user->groupIds();
            if (nx::utils::erase_if(ids, [&user](auto id){ return id == user->getId(); }))
                user->setGroupIds(ids);
        }

        for (auto group: systemContext()->userGroupManager()->groups())
        {
            const auto isTargetGroup = [&group](auto id){ return id == group.id; };
            if (nx::utils::erase_if(group.parentGroupIds, isTargetGroup))
                systemContext()->userGroupManager()->addOrUpdate(group);
        }

        systemContext()->userGroupManager()->remove(id);
    }

    void updateUser(const QnUuid& id, const std::vector<QnUuid>& parents)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto user = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!user.isNull());
        user->setGroupIds(parents);
    }

    void updateGroup(const QnUuid& id, const std::vector<QnUuid>& parents)
    {
        auto group = systemContext()->userGroupManager()->find(id).value_or(api::UserGroupData{});
        ASSERT_EQ(id, group.id);
        group.parentGroupIds = parents;
        systemContext()->userGroupManager()->addOrUpdate(group);
    }

    bool isUserEditable(const QnUuid& id)
    {
        return !systemContext()->nonEditableUsersAndGroups()->containsUser(
            systemContext()->resourcePool()->getResourceById<QnUserResource>(id));
    }

    bool canEditParents(const QnUuid& id)
    {
        return systemContext()->nonEditableUsersAndGroups()->canEditParents(id);
    }

    bool isGroupRemovable(const QnUuid& id)
    {
        return !systemContext()->nonEditableUsersAndGroups()->containsGroup(id);
    }

    bool checkSignalLog(QList<QString> value)
    {
        signalLog.sort();
        value.sort();
        const bool result = value == signalLog;
        signalLog.clear();
        return result;
    }

    void renameUser(const QnUuid& id, const QString& name)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto resource = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!resource.isNull());
        resource->setName(name);
    }

    void enableUser(const QnUuid& id, bool enable)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto resource = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!resource.isNull());
        resource->setEnabled(enable);
    }

    void renameGroup(const QnUuid& id, const QString& name)
    {
        auto group = systemContext()->userGroupManager()->find(id).value_or(api::UserGroupData{});
        ASSERT_EQ(id, group.id);
        group.name = name;
        systemContext()->userGroupManager()->addOrUpdate(group);
    }

    const QSet<QnUuid> kPredefinedGroups = nx::utils::toQSet(api::kPredefinedGroupIds);
    QList<QString> signalLog;
};

TEST_F(NonEditableUsersAndGroupsTest, adminIsNonEditableByPowerUser)
{
    const auto power = addUser("power", {api::kPowerUsersGroupId});
    loginAs(power);

    const auto admin = addUser("admin", {api::kAdministratorsGroupId});

    ASSERT_EQ(kPredefinedGroups, systemContext()->nonEditableUsersAndGroups()->groups());

    ASSERT_FALSE(isUserEditable(admin));
}

TEST_F(NonEditableUsersAndGroupsTest, adminIsNonEditableByAdmin)
{
    const auto admin = addUser("admin", {api::kAdministratorsGroupId});
    loginAs(admin);

    ASSERT_EQ(kPredefinedGroups, systemContext()->nonEditableUsersAndGroups()->groups());

    ASSERT_FALSE(isUserEditable(admin));
}

TEST_F(NonEditableUsersAndGroupsTest, powerUserNotEditableByPowerUser)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto group = addGroup("group", {});
    const auto user = addUser("user", {group});

    ASSERT_TRUE(isUserEditable(user));
    ASSERT_TRUE(isGroupRemovable(group));

    updateGroup(group, {api::kPowerUsersGroupId});

    ASSERT_FALSE(isUserEditable(user));
    ASSERT_FALSE(isGroupRemovable(group));
}

TEST_F(NonEditableUsersAndGroupsTest, usersPreventParentGroupFromDelete)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});

    loginAs(poweruser);

    const auto group1 = addGroup("group1", {});
    const auto group2 = addGroup("group2", {});

    const auto power1 = addUser("power1", {api::kPowerUsersGroupId});
    const auto power2 = addUser("power2", {api::kPowerUsersGroupId});

    ASSERT_TRUE(isGroupRemovable(group1));
    ASSERT_TRUE(isGroupRemovable(group2));

    // Add non-editable user to both groups so they become non-editable.
    updateUser(power1, {api::kPowerUsersGroupId, group1, group2});

    ASSERT_FALSE(isGroupRemovable(group1));
    ASSERT_FALSE(isGroupRemovable(group2));
    ASSERT_TRUE(checkSignalLog({
        "added power1",
        "added power2",
        "added group1",
        "added group2"
        }));

    // Adding another non-editable groups changes nothing.
    updateUser(power2, {api::kPowerUsersGroupId, group1, group2});

    ASSERT_FALSE(isGroupRemovable(group1));
    ASSERT_FALSE(isGroupRemovable(group2));
    ASSERT_TRUE(checkSignalLog({}));

    // Removing the first non-editable groups changes nothing.
    updateUser(power1, {api::kPowerUsersGroupId});

    ASSERT_FALSE(isGroupRemovable(group1));
    ASSERT_FALSE(isGroupRemovable(group2));
    ASSERT_TRUE(checkSignalLog({}));

    // Removing last non-editable user makes the groups editable.
    updateUser(power2, {group1, group2});

    ASSERT_TRUE(isGroupRemovable(group1));
    ASSERT_TRUE(isGroupRemovable(group2));

    ASSERT_TRUE(checkSignalLog({
        "removed power2",
        "removed group1",
        "removed group2"
        }));
}

TEST_F(NonEditableUsersAndGroupsTest, groupPreventsParentGroupFromDelete)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto group = addGroup("group", {});
    const auto subGroup = addGroup("subGroup", {group});

    ASSERT_TRUE(isGroupRemovable(group));

    updateGroup(subGroup, {group, api::kPowerUsersGroupId});

    ASSERT_FALSE(isGroupRemovable(group));
}

TEST_F(NonEditableUsersAndGroupsTest, cycleGroup)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto group = addGroup("group", {});
    const auto subGroup = addGroup("subGroup", {group});
    const auto user = addUser("user", {subGroup});

    ASSERT_TRUE(isUserEditable(user));
    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_TRUE(isGroupRemovable(subGroup));

    // Introduce group <-> subGroup cycle and make PowerUsers a parent, so all become non-editable.
    updateGroup(group, {api::kPowerUsersGroupId, subGroup});

    ASSERT_FALSE(isUserEditable(user));
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_FALSE(isGroupRemovable(subGroup));
    ASSERT_TRUE(checkSignalLog({
        "added user",
        "added group",
        "added subGroup"
        }));

    // Remove a cycle and PowerUsers, all becomes editable.
    updateGroup(group, {});

    ASSERT_TRUE(isUserEditable(user));
    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_TRUE(isGroupRemovable(subGroup));

    ASSERT_TRUE(checkSignalLog({
        "removed user",
        "removed group",
        "removed subGroup"
        }));
}

TEST_F(NonEditableUsersAndGroupsTest, nonEditablePropagatesDown)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    /* When 'group' becomes non-editable, only 'sideGroup1' should remain editable.

      PowerUsers
          |
        group       sideGroup1
          |             |
        subGroup    sideGroup2
                \   /
                user
    */

    const auto sideGroup1 = addGroup("sideGroup1", {});
    const auto sideGroup2 = addGroup("sideGroup2", {sideGroup1});

    const auto group = addGroup("group", {});
    const auto subGroup = addGroup("subGroup", {group});
    const auto user = addUser("user", {subGroup, sideGroup2});

    ASSERT_TRUE(isUserEditable(user));
    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_TRUE(isGroupRemovable(subGroup));
    ASSERT_TRUE(isGroupRemovable(sideGroup1));
    ASSERT_TRUE(isGroupRemovable(sideGroup2));

    // Add PowerUsers parent.
    updateGroup(group, {api::kPowerUsersGroupId});

    ASSERT_FALSE(isUserEditable(user));
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_FALSE(isGroupRemovable(subGroup));
    ASSERT_TRUE(isGroupRemovable(sideGroup1)); //< Remains editable.
    ASSERT_FALSE(isGroupRemovable(sideGroup2));
    ASSERT_EQ(
        (kPredefinedGroups + QSet{group, subGroup, sideGroup2}),
        systemContext()->nonEditableUsersAndGroups()->groups());

    // User and poweruser are non-editable.
    ASSERT_EQ(2, systemContext()->nonEditableUsersAndGroups()->userCount());

    ASSERT_TRUE(checkSignalLog({
        "added group",
        "added subGroup",
        "added sideGroup2",
        "added user"
        }));

    // Remove PowerUsers parent.
    updateGroup(group, {});

    ASSERT_TRUE(isUserEditable(user));
    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_TRUE(isGroupRemovable(subGroup));
    ASSERT_TRUE(isGroupRemovable(sideGroup1));
    ASSERT_TRUE(isGroupRemovable(sideGroup2));
    ASSERT_TRUE(checkSignalLog({
        "removed group",
        "removed subGroup",
        "removed sideGroup2",
        "removed user"
        }));
}

TEST_F(NonEditableUsersAndGroupsTest, newUserPermissionsAreMonitored)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto user = addUser("user", {});

    ASSERT_TRUE(isUserEditable(user));
    updateUser(user, {api::kPowerUsersGroupId});
    ASSERT_FALSE(isUserEditable(user));
}

TEST_F(NonEditableUsersAndGroupsTest, newGroupPermissionsAreMonitored)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});

    loginAs(poweruser);

    const auto group = addGroup("group", {});

    ASSERT_TRUE(isGroupRemovable(group));
    updateGroup(group, {api::kPowerUsersGroupId});
    ASSERT_FALSE(isGroupRemovable(group));
}

TEST_F(NonEditableUsersAndGroupsTest, nonUniqueUserRenameMakesGroupEditable)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto user1 = addUser("user", {});
    const auto user2 = addUser("user", {});
    const auto group = addGroup("group", {});

    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_FALSE(canEditParents(user1));
    ASSERT_FALSE(canEditParents(user2));

    // Add one of the duplicate users to the group - this makes the group imposible to delete
    // because it cannot be removed form parent groups list of this user.
    updateUser(user1, {group});
    ASSERT_FALSE(isGroupRemovable(group));

    // Renaming the other duplicate make everything editable again.
    renameUser(user2, "user1");
    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_TRUE(canEditParents(user1));
    ASSERT_TRUE(canEditParents(user2));
}

TEST_F(NonEditableUsersAndGroupsTest, nonUniqueUserDisableMakesGroupEditable)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto user1 = addUser("user", {});
    const auto user2 = addUser("user", {});
    const auto group = addGroup("group", {});

    ASSERT_TRUE(isGroupRemovable(group));

    updateUser(user1, {group});
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_FALSE(canEditParents(user1));
    ASSERT_FALSE(canEditParents(user2));

    // Disable one of the duplicates.
    enableUser(user2, false);
    ASSERT_TRUE(isGroupRemovable(group));
    // Both users are editabled because only of the duplicates (user1) is enabled.
    ASSERT_TRUE(canEditParents(user1));
    ASSERT_TRUE(canEditParents(user2));
}

TEST_F(NonEditableUsersAndGroupsTest, nonUniqueUserRemainsMassEditable)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto user1 = addUser("user", {});
    addUser("user", {});
    const auto group = addGroup("group", {});

    signalLog.clear();

    ASSERT_TRUE(isGroupRemovable(group));

    updateUser(user1, {group});
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_TRUE(checkSignalLog({
        "added group",
        }));

    // user1 is not editable because its name is duplicated and both users are enabled.
    // Verify that adding a duplicated user to the group that prevents editing this user still
    // emits a signal about user modification.
    updateUser(user1, {group, api::kPowerUsersGroupId});
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_TRUE(checkSignalLog({
        "added user",
        }));
}

TEST_F(NonEditableUsersAndGroupsTest, nonUniqueUserBecomesMassEditable)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto user1 = addUser("user", {});
    const auto user2 = addUser("user", {});
    const auto group = addGroup("group", {});

    signalLog.clear();

    ASSERT_TRUE(isGroupRemovable(group));

    // Make user1 non-editable also by adding it to Power Users group.
    updateUser(user1, {group, api::kPowerUsersGroupId});
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_FALSE(isUserEditable(user1));
    ASSERT_TRUE(checkSignalLog({
        "added group",
        "added user",
        }));

    // Verify that renaming a duplicate does not make user1 editable.
    renameUser(user2, "user2");
    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_FALSE(isUserEditable(user1));
    ASSERT_TRUE(isUserEditable(user2));
    ASSERT_TRUE(checkSignalLog({
        "removed user2",
        }));
}

TEST_F(NonEditableUsersAndGroupsTest, nonUniqueGroups)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto group1 = addGroup("group", {});
    const auto group2 = addGroup("group", {});

    ASSERT_FALSE(canEditParents(group1));
    ASSERT_FALSE(canEditParents(group2));

    ASSERT_TRUE(isGroupRemovable(group1));
    ASSERT_TRUE(isGroupRemovable(group2));

    renameGroup(group1, "group1");

    ASSERT_TRUE(canEditParents(group1));
    ASSERT_TRUE(canEditParents(group2));

    ASSERT_TRUE(isGroupRemovable(group1));
    ASSERT_TRUE(isGroupRemovable(group2));
}

TEST_F(NonEditableUsersAndGroupsTest, cannotDeleteGroupWithNonUniqueSubgroups)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto subGroup1 = addGroup("subGroup", {});
    const auto subGroup2 = addGroup("subGroup", {});
    const auto group = addGroup("group", {});

    signalLog.clear();

    // Make group non-deletable by adding a duplicate.
    updateGroup(subGroup1, {group});

    ASSERT_FALSE(isGroupRemovable(group));
    ASSERT_TRUE(canEditParents(group));

    ASSERT_TRUE(checkSignalLog({
        "added group",
        }));

    renameGroup(subGroup2, "subGroup1");

    ASSERT_TRUE(canEditParents(subGroup1));
    ASSERT_TRUE(canEditParents(subGroup2));

    ASSERT_TRUE(isGroupRemovable(subGroup1));
    ASSERT_TRUE(isGroupRemovable(subGroup2));

    ASSERT_TRUE(isGroupRemovable(group));
    ASSERT_TRUE(canEditParents(group));

    ASSERT_TRUE(checkSignalLog({
        "removed subGroup1",
        "removed subGroup",
        "removed group",
        }));
}

TEST_F(NonEditableUsersAndGroupsTest, allowDeleteLdapGroupWithNonUniqueLdapSubgroups)
{
    const auto poweruser = addUser("poweruser", {api::kPowerUsersGroupId});
    loginAs(poweruser);

    const auto group1 = addGroup("group1", {}, nx::vms::api::UserType::ldap);
    const auto group2 = addGroup("group2", {});
    const auto subGroup1 = addGroup("subGroup", {group1}, nx::vms::api::UserType::ldap);
    const auto subGroup2 = addGroup("subGroup", {group1, group2}, nx::vms::api::UserType::ldap);
    const auto subGroup3 = addGroup("subGroup", {group2});

    ASSERT_FALSE(canEditParents(subGroup1));
    ASSERT_FALSE(canEditParents(subGroup2));
    ASSERT_FALSE(canEditParents(subGroup3));

    // Removing an LDAP group is allowed without removing it from parent group ids of its members.
    ASSERT_TRUE(isGroupRemovable(group1));

    ASSERT_FALSE(isGroupRemovable(group2));

    // Renaming subGroup3 does not make group2 removable because group2 still contains duplicated
    // subGroup2.
    renameGroup(subGroup3, "subGroup3");
    ASSERT_FALSE(isGroupRemovable(group2));

    // Renaming subGroup1 makes subGroup2 unique which in turn makes group2 removable.
    renameGroup(subGroup1, "subGroup1");
    ASSERT_TRUE(isGroupRemovable(group2));
}

} // namespace test

} // namespace nx::vms::client::desktop
