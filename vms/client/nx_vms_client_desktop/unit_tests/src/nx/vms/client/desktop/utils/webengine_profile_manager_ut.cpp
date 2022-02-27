// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <client/client_module.h>
#include <nx/vms/client/desktop/utils/webengine_profile_manager.h>

namespace nx::vms::client::desktop::utils {

namespace test {

class WebEngineProfileManagerTest: public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        QnClientModule::initWebEngine();
    }
};

TEST_F(WebEngineProfileManagerTest, sameProfileForSingleUser)
{
    const auto profile1 = WebEngineProfileManager::instance()->getProfile("user", true);
    const auto profile2 = WebEngineProfileManager::instance()->getProfile("user", true);

    ASSERT_EQ(profile1, profile2);
}

TEST_F(WebEngineProfileManagerTest, differentProfilesForDifferentUsers)
{
    const auto profile1 = WebEngineProfileManager::instance()->getProfile("user1", true);
    const auto profile2 = WebEngineProfileManager::instance()->getProfile("user2", true);

    ASSERT_NE(profile1, profile2);
}

TEST_F(WebEngineProfileManagerTest, offTheRecordProfileForEmptyUser)
{
    const auto profile = WebEngineProfileManager::instance()->getProfile("user", false);
    const auto profileEmpty = WebEngineProfileManager::instance()->getProfile("user", true);

    ASSERT_FALSE(profile->isOffTheRecord());
    ASSERT_TRUE(profileEmpty->isOffTheRecord());
}

} // namespace test

} // namespace nx::vms::client::desktop::utils
