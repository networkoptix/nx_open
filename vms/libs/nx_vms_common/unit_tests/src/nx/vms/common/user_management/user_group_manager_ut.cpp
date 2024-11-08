// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtTest/QSignalSpy>

#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

using namespace nx::vms::api;

namespace nx::vms::common {
namespace test {

static UserGroupDataList operator+(const UserGroupDataList& l, const UserGroupDataList& r)
{
    UserGroupDataList result(l);
    result.insert(result.end(), r.begin(), r.end());
    return result;
}

UserGroupDataList sorted(UserGroupDataList list)
{
    std::sort(list.begin(), list.end(), [](const auto& l, const auto& r) { return l.id < r.id; });
    return list;
}

#define SORT_AND_EXPECT_EQ(list1, list2) \
    EXPECT_EQ(sorted(std::move(list1)), sorted(std::move(list2)))

class UserGroupManagerTest: public ::testing::Test
{
public:
    virtual void SetUp() { manager.reset(new UserGroupManager()); }
    virtual void TearDown() { manager.reset(); }

protected:
    std::unique_ptr<UserGroupManager> manager;

    const UserGroupData group1{nx::Uuid::createUuid(), "group1"};
    const UserGroupData group2{nx::Uuid::createUuid(), "group2", {}, {kViewersGroupId}};
    const UserGroupData group3{nx::Uuid::createUuid(), "group3", {}, {kPowerUsersGroupId}};
    const UserGroupData group4{nx::Uuid::createUuid(), "group4", {}, {group1.id, group2.id}};

    const UserGroupDataList testGroups{{group1, group2, group3, group4}};
};

TEST_F(UserGroupManagerTest, ids)
{
    EXPECT_EQ(manager->ids(), PredefinedUserGroups::ids());
    EXPECT_EQ(manager->ids(UserGroupManager::Selection::predefined),
        PredefinedUserGroups::ids());
    EXPECT_EQ(manager->ids(UserGroupManager::Selection::custom), QList<nx::Uuid>{});

    ASSERT_TRUE(manager->addOrUpdate(group1));

    EXPECT_EQ(manager->ids(), PredefinedUserGroups::ids() + QList<nx::Uuid>{group1.id});
    EXPECT_EQ(manager->ids(UserGroupManager::Selection::predefined),
        PredefinedUserGroups::ids());
    EXPECT_EQ(manager->ids(UserGroupManager::Selection::custom), QList<nx::Uuid>{group1.id});

    EXPECT_EQ(manager->ids(), manager->ids(UserGroupManager::Selection::all));
}

TEST_F(UserGroupManagerTest, groups)
{
    EXPECT_EQ(manager->groups(), PredefinedUserGroups::list());
    EXPECT_EQ(manager->groups(UserGroupManager::Selection::predefined),
        PredefinedUserGroups::list());
    EXPECT_EQ(manager->groups(UserGroupManager::Selection::custom), UserGroupDataList{});

    ASSERT_TRUE(manager->addOrUpdate(group1));

    EXPECT_EQ(manager->groups(), PredefinedUserGroups::list() + UserGroupDataList{group1});
    EXPECT_EQ(manager->groups(UserGroupManager::Selection::predefined),
        PredefinedUserGroups::list());
    EXPECT_EQ(manager->groups(UserGroupManager::Selection::custom), UserGroupDataList{group1});

    EXPECT_EQ(manager->groups(), manager->groups(UserGroupManager::Selection::all));
}

TEST_F(UserGroupManagerTest, resetAll)
{
    const QSignalSpy spy(manager.get(), &UserGroupManager::reset);
    manager->resetAll(testGroups);
    EXPECT_EQ(spy.count(), 1);

    SORT_AND_EXPECT_EQ(manager->groups(UserGroupManager::Selection::custom), testGroups);
    SORT_AND_EXPECT_EQ(manager->groups(), PredefinedUserGroups::list() + testGroups);
}

TEST_F(UserGroupManagerTest, resetIgnoresPredefinedGroups)
{
    manager->resetAll(testGroups + PredefinedUserGroups::list());
    SORT_AND_EXPECT_EQ(manager->groups(UserGroupManager::Selection::custom), testGroups);
    SORT_AND_EXPECT_EQ(manager->groups(), PredefinedUserGroups::list() + testGroups);
}

TEST_F(UserGroupManagerTest, find)
{
    manager->resetAll(testGroups);

    for (const auto& group: manager->groups())
    {
        const auto found = manager->find(group.id);
        ASSERT_TRUE((bool) found);
        EXPECT_EQ(group, *found);
    }

    EXPECT_FALSE((bool) manager->find(nx::Uuid{}));
    EXPECT_FALSE((bool)manager->find(nx::Uuid::createUuid()));
}

TEST_F(UserGroupManagerTest, contains)
{
    for (const auto& id: PredefinedUserGroups::ids())
        EXPECT_TRUE(manager->contains(id));

    manager->resetAll(testGroups);

    for (const auto& id: PredefinedUserGroups::ids())
        EXPECT_TRUE(manager->contains(id));

    for (const auto& group: testGroups)
        EXPECT_TRUE(manager->contains(group.id));

    EXPECT_FALSE(manager->contains(nx::Uuid{}));
    EXPECT_FALSE(manager->contains(nx::Uuid::createUuid()));
}

TEST_F(UserGroupManagerTest, addAndUpdate)
{
    // Add.
    QSignalSpy spy(manager.get(), &UserGroupManager::addedOrUpdated);
    EXPECT_TRUE(manager->addOrUpdate(group2));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst(), QVariantList{{QVariant::fromValue(group2)}});

    // Update.
    auto group2a = group2;
    group2a.name = "group2a";
    EXPECT_TRUE(manager->addOrUpdate(group2a));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst(), QVariantList{{QVariant::fromValue(group2a)}});

    // No change produces no update.
    EXPECT_FALSE(manager->addOrUpdate(group2a));
    EXPECT_EQ(manager->find(group2.id)->name, group2a.name);
    ASSERT_TRUE(spy.empty());

    // Cannot change a predefined group.
    for (auto predefined: PredefinedUserGroups::list())
    {
        predefined.name = "Modified Predefined";
        EXPECT_FALSE(manager->addOrUpdate(predefined));
        EXPECT_NE(manager->find(predefined.id)->name, predefined.name);
        ASSERT_TRUE(spy.empty());
    }

    // Cannot add an invalid group.
    EXPECT_FALSE(manager->addOrUpdate(UserGroupData{}));
    EXPECT_FALSE(manager->contains(nx::Uuid{}));
    ASSERT_TRUE(spy.empty());
}

TEST_F(UserGroupManagerTest, remove)
{
    QSignalSpy spy(manager.get(), &UserGroupManager::removed);

    // Cannot remove a predefined group.
    for (const auto& id: PredefinedUserGroups::ids())
        EXPECT_FALSE(manager->remove(id));

    ASSERT_TRUE(spy.empty());

    // Cannot remove an invalid group.
    EXPECT_FALSE(manager->remove(nx::Uuid{}));
    ASSERT_TRUE(spy.empty());

    // Cannot remove a non-existing group.
    EXPECT_FALSE(manager->remove(nx::Uuid::createUuid()));
    ASSERT_TRUE(spy.empty());

    manager->resetAll(testGroups);

    // Can remove existing custom groups.
    for (const auto& group: testGroups)
    {
        EXPECT_TRUE(manager->contains(group.id));
        EXPECT_TRUE(manager->remove(group.id));
        EXPECT_FALSE(manager->contains(group.id));
        ASSERT_EQ(spy.count(), 1);
        EXPECT_EQ(spy.takeFirst(), QVariantList{{QVariant::fromValue(group)}});
    }
}

TEST_F(UserGroupManagerTest, getGroupsByIds)
{
    const std::vector<nx::Uuid> requestedIds{{
        nx::Uuid::createUuid(),
        nx::Uuid::createUuid(),
        group1.id,
        nx::Uuid{},
        nx::Uuid::createUuid(),
        group3.id,
        kViewersGroupId,
        nx::Uuid::createUuid(),
        nx::Uuid::createUuid(),
        kAdministratorsGroupId,
        group4.id,
        nx::Uuid{},
        nx::Uuid::createUuid()}};

    const UserGroupDataList expectedPredefinedGroups{{
        *PredefinedUserGroups::find(kViewersGroupId),
        *PredefinedUserGroups::find(kAdministratorsGroupId)}};

    EXPECT_EQ(manager->getGroupsByIds(requestedIds), expectedPredefinedGroups);

    const UserGroupDataList expectedGroups{{
        group1,
        group3,
        *PredefinedUserGroups::find(kViewersGroupId),
        *PredefinedUserGroups::find(kAdministratorsGroupId),
        group4}};

    manager->resetAll(testGroups);
    EXPECT_EQ(manager->getGroupsByIds(requestedIds), expectedGroups);
}

} // namespace test
} // namespace nx::vms::common
