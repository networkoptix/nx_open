// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFileInfo>

#include <nx/utils/external_resources.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::utils;

class AnalyticsIconManagerTest: public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        ASSERT_TRUE(registerExternalResource("client_core_external.dat"));
        ASSERT_TRUE(registerExternalResource("bytedance_iconpark.dat",
            core::analytics::IconManager::librariesRoot() + "bytedance.iconpark/"));
    }

    static void TearDownTestCase()
    {
        ASSERT_TRUE(unregisterExternalResource("client_core_external.dat"));
        ASSERT_TRUE(unregisterExternalResource("bytedance_iconpark.dat",
            core::analytics::IconManager::librariesRoot() + "bytedance.iconpark/"));
    }
};

TEST_F(AnalyticsIconManagerTest, taxonomyBaseTypeLibrary)
{
    const auto testBaseIconPath = core::analytics::IconManager::instance()->absoluteIconPath(
        "nx.base.car");
    ASSERT_EQ(testBaseIconPath, QString(":/skin/analytics_icons/nx.base/car.svg"));
    ASSERT_TRUE(QFileInfo::exists(testBaseIconPath));
}

TEST_F(AnalyticsIconManagerTest, thirdPartyLibrary)
{
    const auto test3rdPartyIconPath = core::analytics::IconManager::instance()->absoluteIconPath(
        "bytedance.iconpark.palace");
    ASSERT_EQ(test3rdPartyIconPath, QString(":/skin/analytics_icons/bytedance.iconpark/palace.svg"));
    ASSERT_TRUE(QFileInfo::exists(test3rdPartyIconPath));
}

TEST_F(AnalyticsIconManagerTest, fallbackIconExists)
{
    ASSERT_TRUE(QFileInfo::exists(core::analytics::IconManager::skinRoot()
        + core::analytics::IconManager::instance()->fallbackIconPath()));
}

} // namespace test
} // namespace nx::vms::client::desktop
