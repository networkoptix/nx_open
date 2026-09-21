// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/mobile/push_notification/details/user_push_settings_helpers.h>
#include <nx/vms/client/mobile/push_notification/push_notification_model.h>

namespace nx::vms::client::mobile::details::test {

namespace {

using Filter = PushNotificationFilterModel::Filter;

constexpr auto kLowerCaseUser = "user@example.com";
constexpr auto kMixedCaseUser = "User@Example.COM";
constexpr auto kUpperCaseUser = "USER@EXAMPLE.COM";
constexpr auto kOtherUser = "other@example.com";

LocalPushSettings makeSettings(Filter lastUsedFilter)
{
    return LocalPushSettings(
        /*enabled*/ true, allSystemsModeValue(), TokenData::makeEmpty(), lastUsedFilter);
}

} // namespace

TEST(UserPushSettingsHelpers, findByNormalizedKeyRegardlessOfCase)
{
    const UserPushSettings settings{{kLowerCaseUser, makeSettings(Filter::Viewed)},
        {kOtherUser, makeSettings(Filter::Unviewed)}};

    EXPECT_EQ(findUserPushSettings(settings, kLowerCaseUser), makeSettings(Filter::Viewed));
    EXPECT_EQ(findUserPushSettings(settings, kMixedCaseUser), makeSettings(Filter::Viewed));
    EXPECT_EQ(findUserPushSettings(settings, kOtherUser), makeSettings(Filter::Unviewed));
    EXPECT_FALSE(findUserPushSettings(settings, "unknown@example.com").has_value());
    EXPECT_FALSE(findUserPushSettings(settings, "").has_value());
}

TEST(UserPushSettingsHelpers, findLegacyNonNormalizedEntry)
{
    const UserPushSettings settings{{kMixedCaseUser, makeSettings(Filter::Viewed)}};

    EXPECT_EQ(findUserPushSettings(settings, kMixedCaseUser), makeSettings(Filter::Viewed));
    EXPECT_EQ(findUserPushSettings(settings, kLowerCaseUser), makeSettings(Filter::Viewed));
    EXPECT_EQ(findUserPushSettings(settings, kUpperCaseUser), makeSettings(Filter::Viewed));
}

TEST(UserPushSettingsHelpers, normalizedEntryIsPreferredOverLegacyOne)
{
    const UserPushSettings settings{{kLowerCaseUser, makeSettings(Filter::Viewed)},
        {kMixedCaseUser, makeSettings(Filter::Unviewed)}};

    EXPECT_EQ(findUserPushSettings(settings, kMixedCaseUser), makeSettings(Filter::Viewed));
}

TEST(UserPushSettingsHelpers, updateStoresUnderNormalizedKey)
{
    UserPushSettings settings;

    EXPECT_TRUE(updateUserPushSettings(settings, kMixedCaseUser, makeSettings(Filter::Viewed)));
    ASSERT_EQ(settings.size(), 1);
    EXPECT_TRUE(settings.contains(kLowerCaseUser));
    EXPECT_EQ(settings.at(kLowerCaseUser), makeSettings(Filter::Viewed));

    EXPECT_TRUE(updateUserPushSettings(settings, kLowerCaseUser, makeSettings(Filter::Unviewed)));
    ASSERT_EQ(settings.size(), 1);
    EXPECT_EQ(settings.at(kLowerCaseUser), makeSettings(Filter::Unviewed));
}

TEST(UserPushSettingsHelpers, updateMigratesLegacyEntries)
{
    UserPushSettings settings{{kMixedCaseUser, makeSettings(Filter::Viewed)},
        {kUpperCaseUser, makeSettings(Filter::Viewed)},
        {kOtherUser, makeSettings(Filter::Unviewed)}};

    EXPECT_TRUE(updateUserPushSettings(settings, kLowerCaseUser, makeSettings(Filter::All)));

    ASSERT_EQ(settings.size(), 2);
    EXPECT_EQ(settings.at(kLowerCaseUser), makeSettings(Filter::All));
    EXPECT_EQ(settings.at(kOtherUser), makeSettings(Filter::Unviewed));
}

TEST(UserPushSettingsHelpers, removeDropsAllSpellings)
{
    UserPushSettings settings{{kLowerCaseUser, makeSettings(Filter::Viewed)},
        {kMixedCaseUser, makeSettings(Filter::Viewed)},
        {kOtherUser, makeSettings(Filter::Unviewed)}};

    EXPECT_TRUE(updateUserPushSettings(settings, kUpperCaseUser, std::nullopt));
    ASSERT_EQ(settings.size(), 1);
    EXPECT_EQ(settings.at(kOtherUser), makeSettings(Filter::Unviewed));

    EXPECT_FALSE(updateUserPushSettings(settings, kLowerCaseUser, std::nullopt));
    EXPECT_EQ(settings.size(), 1);
}

TEST(UserPushSettingsHelpers, emptyUserIsIgnored)
{
    UserPushSettings settings{{kOtherUser, makeSettings(Filter::Viewed)}};

    EXPECT_FALSE(updateUserPushSettings(settings, "", makeSettings(Filter::Unviewed)));
    EXPECT_FALSE(updateUserPushSettings(settings, "", std::nullopt));
    EXPECT_EQ(settings.size(), 1);
}

} // namespace nx::vms::client::mobile::details::test
