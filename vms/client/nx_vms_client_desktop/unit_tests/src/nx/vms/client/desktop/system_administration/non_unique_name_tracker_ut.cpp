// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/system_administration/watchers/private/non_unique_name_tracker.h>

namespace nx::vms::client::desktop::test {

namespace {

const auto kId1 = QnUuid::createUuid();
const auto kId2 = QnUuid::createUuid();
const auto kId3 = QnUuid::createUuid();
const QString kName1 = "name1";
const QString kName2 = "name2";
const QString kName3 = "name3";

} // namespace

TEST(NonUniqueNameTrackerTest, uniqueNames)
{
    NonUniqueNameTracker tracker;
    EXPECT_FALSE(tracker.update(kId1, kName1));
    EXPECT_FALSE(tracker.update(kId2, kName2));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 0);
}

TEST(NonUniqueNameTrackerTest, updatingName)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    EXPECT_FALSE(tracker.update(kId1, kName2));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 0);
    EXPECT_TRUE(tracker.isUnique(kId1));
}

TEST(NonUniqueNameTrackerTest, trivialNonUnique)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    EXPECT_TRUE(tracker.update(kId2, kName1));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 2);
    EXPECT_FALSE(tracker.isUnique(kId1));
    EXPECT_FALSE(tracker.isUnique(kId2));

    EXPECT_TRUE(tracker.update(kId3, kName1));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 3);
    EXPECT_FALSE(tracker.isUnique(kId3));
}

TEST(NonUniqueNameTrackerTest, removingNonUniqueName)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    tracker.update(kId2, kName1);
    EXPECT_TRUE(tracker.remove(kId2));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 0);
    EXPECT_TRUE(tracker.isUnique(kId1));
}

TEST(NonUniqueNameTrackerTest, removingNonUniqueName2)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    tracker.update(kId2, kName1);
    tracker.update(kId3, kName1);
    EXPECT_TRUE(tracker.remove(kId3));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 2);
    EXPECT_FALSE(tracker.isUnique(kId1));
    EXPECT_FALSE(tracker.isUnique(kId2));
}

TEST(NonUniqueNameTrackerTest, updatingNonUniqueName)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    tracker.update(kId2, kName1);
    EXPECT_TRUE(tracker.update(kId2, kName2));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 0);
    EXPECT_TRUE(tracker.isUnique(kId1));
    EXPECT_TRUE(tracker.isUnique(kId2));
}

TEST(NonUniqueNameTrackerTest, updatingNonUniqueName2)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    tracker.update(kId2, kName1);
    tracker.update(kId3, kName1);
    EXPECT_TRUE(tracker.update(kId3, kName3));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 2);
    EXPECT_FALSE(tracker.isUnique(kId1));
    EXPECT_FALSE(tracker.isUnique(kId2));
    EXPECT_TRUE(tracker.isUnique(kId3));
}

TEST(NonUniqueNameTrackerTest, updateMakesNonUniqueName)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);
    tracker.update(kId2, kName2);
    EXPECT_TRUE(tracker.update(kId2, kName1));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 2);
    EXPECT_FALSE(tracker.isUnique(kId1));
    EXPECT_FALSE(tracker.isUnique(kId2));
}

TEST(NonUniqueNameTrackerTest, degenerateUpdates)
{
    NonUniqueNameTracker tracker;
    tracker.update(kId1, kName1);

    tracker.update(kId2, kName2);
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 0);
    EXPECT_FALSE(tracker.update(kId2, kName2));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 0);

    tracker.update(kId2, kName1);
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 2);
    EXPECT_FALSE(tracker.update(kId2, kName1));
    EXPECT_EQ(tracker.nonUniqueNameIds().size(), 2);
}

} // namespace nx::vms::client::desktop::test
