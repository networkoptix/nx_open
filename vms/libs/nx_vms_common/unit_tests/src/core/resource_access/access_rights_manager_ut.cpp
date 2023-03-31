// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <memory>

#include <QtTest/QSignalSpy>

#include <core/resource_access/access_rights_manager.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>

using namespace nx::vms::api;
using namespace nx::vms::common;

using nx::utils::makeQVariantList;

namespace nx::core::access {
namespace test {

class AccessRightsManagerTest: public ::testing::Test
{
public:
    virtual void SetUp()
    {
        manager.reset(new AccessRightsManager());
        resetSpy.reset(new QSignalSpy(manager.get(), &AccessRightsManager::accessRightsReset));
        changeSpy.reset(new QSignalSpy(manager.get(), &AccessRightsManager::ownAccessRightsChanged));
    }

    virtual void TearDown()
    {
        changeSpy.reset();
        resetSpy.reset();
        manager.reset();
    }

    void checkPredefinedGroupAccessRights()
    {
        for (const auto& id: PredefinedUserGroups::ids())
            EXPECT_EQ(manager->ownResourceAccessMap(id), PredefinedUserGroups::accessRights(id));
    }

protected:
    std::unique_ptr<AccessRightsManager> manager;
    std::unique_ptr<QSignalSpy> resetSpy;
    std::unique_ptr<QSignalSpy> changeSpy;

    const QnUuid subject1 = QnUuid::createUuid();
    const QnUuid subject2 = QnUuid::createUuid();
    const QnUuid subject3 = QnUuid::createUuid();

    const QnUuid resource1 = QnUuid::createUuid();
    const QnUuid resource2 = QnUuid::createUuid();
    const QnUuid resource3 = QnUuid::createUuid();

    const ResourceAccessMap testRights1{{resource1, AccessRight::view}};
    const ResourceAccessMap testRights2{{resource2, AccessRight::view | AccessRight::viewArchive},
        {resource3, AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks}};
    const ResourceAccessMap testRights3{{kAllDevicesGroupId, AccessRight::view}};
};

TEST_F(AccessRightsManagerTest, predefinedGroupRights) //< Also a test for ownResourceAccessMap().
{
    checkPredefinedGroupAccessRights();
}

TEST_F(AccessRightsManagerTest, resetAndClear)
{
    manager->resetAccessRights({
        {subject1, testRights1},
        {subject2, testRights2},
        {subject3, testRights3}});

    EXPECT_EQ(resetSpy->size(), 1);
    EXPECT_TRUE(changeSpy->empty());
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), testRights1);
    EXPECT_EQ(manager->ownResourceAccessMap(subject2), testRights2);
    EXPECT_EQ(manager->ownResourceAccessMap(subject3), testRights3);

    // Predefined group access rights persist through a reset.
    checkPredefinedGroupAccessRights();

    manager->clear();
    EXPECT_EQ(resetSpy->size(), 2);
    EXPECT_TRUE(changeSpy->empty());
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), ResourceAccessMap());
    EXPECT_EQ(manager->ownResourceAccessMap(subject2), ResourceAccessMap());
    EXPECT_EQ(manager->ownResourceAccessMap(subject3), ResourceAccessMap());

    // Predefined group access rights persist through a clear.
    checkPredefinedGroupAccessRights();
}

TEST_F(AccessRightsManagerTest, set)
{
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), ResourceAccessMap());

    // Add resource access map for a subject.
    EXPECT_TRUE(manager->setOwnResourceAccessMap(subject1, testRights1));
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), testRights1);
    EXPECT_TRUE(resetSpy->empty());
    ASSERT_EQ(changeSpy->size(), 1);
    EXPECT_EQ(changeSpy->takeLast(), makeQVariantList(QSet<QnUuid>{subject1}));

    // Modify resource access map for a subject.
    EXPECT_TRUE(manager->setOwnResourceAccessMap(subject1, testRights2));
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), testRights2);
    EXPECT_TRUE(resetSpy->empty());
    ASSERT_EQ(changeSpy->size(), 1);
    EXPECT_EQ(changeSpy->takeLast(), makeQVariantList(QSet<QnUuid>{subject1}));

    // Set the same resource access map for a subject.
    EXPECT_FALSE(manager->setOwnResourceAccessMap(subject1, testRights2));
    EXPECT_TRUE(resetSpy->empty());
    EXPECT_TRUE(changeSpy->empty()); //< No actual change.
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), testRights2);

    EXPECT_FALSE(manager->setOwnResourceAccessMap(subject2, ResourceAccessMap()));
    EXPECT_TRUE(resetSpy->empty());
    EXPECT_TRUE(changeSpy->empty()); //< Also no actual change.
    EXPECT_EQ(manager->ownResourceAccessMap(subject2), ResourceAccessMap());

    // Cannot modify access rights of a predefined user group.
    EXPECT_FALSE(manager->setOwnResourceAccessMap(kLiveViewersGroupId, kAdminResourceAccessMap));
    EXPECT_TRUE(resetSpy->empty());
    EXPECT_TRUE(changeSpy->empty());
    EXPECT_NE(manager->ownResourceAccessMap(kLiveViewersGroupId), kAdminResourceAccessMap);
}

TEST_F(AccessRightsManagerTest, remove)
{
    manager->resetAccessRights({
        {subject1, testRights1},
        {subject2, testRights2},
        {subject3, testRights3}});

    resetSpy->clear();

    // Remove subject1 and subject3.
    EXPECT_TRUE(manager->removeSubjects({subject1, subject3}));
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), ResourceAccessMap());
    EXPECT_EQ(manager->ownResourceAccessMap(subject2), testRights2);
    EXPECT_EQ(manager->ownResourceAccessMap(subject3), ResourceAccessMap());
    EXPECT_TRUE(resetSpy->empty());
    ASSERT_EQ(changeSpy->size(), 1);
    EXPECT_EQ(changeSpy->takeLast(), makeQVariantList(QSet<QnUuid>{subject1, subject3}));

    // Try to remove subject1 and subject2, but subject1 is already removed. Remove some.
    EXPECT_TRUE(manager->removeSubjects({subject1, subject2}));
    EXPECT_EQ(manager->ownResourceAccessMap(subject1), ResourceAccessMap());
    EXPECT_EQ(manager->ownResourceAccessMap(subject2), ResourceAccessMap());
    EXPECT_EQ(manager->ownResourceAccessMap(subject3), ResourceAccessMap());
    EXPECT_TRUE(resetSpy->empty());
    ASSERT_EQ(changeSpy->size(), 1);
    EXPECT_EQ(changeSpy->takeLast(), makeQVariantList(QSet<QnUuid>{subject2}));

    // Try to remove subject1 and subject2, but both are already removed. Remove none.
    EXPECT_FALSE(manager->removeSubjects({subject1, subject2}));
    EXPECT_TRUE(resetSpy->empty());
    EXPECT_TRUE(changeSpy->empty());

    // Cannot remove predefined user groups.
    EXPECT_FALSE(manager->removeSubjects(nx::utils::toQSet(kPredefinedGroupIds)));
    EXPECT_TRUE(resetSpy->empty());
    ASSERT_TRUE(changeSpy->empty());
    checkPredefinedGroupAccessRights();
}

} // namespace test
} // namespace nx::core::access
