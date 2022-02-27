// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>
#include <nx/vms/license/usage_helper.h>

#include "license_pool_scaffold.h"
#include "license_stub.h"

namespace nx::vms::license::test {

class QnVideowallLicenseUsageHelperTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_staticCommon = std::make_unique<QnStaticCommonModule>();
        m_module = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::direct);
        initializeContext(m_module.get());

        m_licenses.reset(new QnLicensePoolScaffold(licensePool()));
        m_helper.reset(new VideoWallLicenseUsageHelper(commonModule()));
        m_helper->setCustomValidator(std::make_unique<QLicenseStubValidator>(commonModule()));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        m_helper.reset();
        m_licenses.reset();
        m_module.reset();
        m_staticCommon.reset();
    }

    void addLicense()
    {
        m_licenses->addLicense(Qn::LC_VideoWall);
    }

    void addLicenses(int count)
    {
        m_licenses->addLicenses(Qn::LC_VideoWall, count);
    }

    QnVideoWallResourcePtr addVideoWallWithItems(int itemCount)
    {
        auto videowall = createVideoWall();
        for (int i = 0; i < itemCount; ++i)
            addItem(videowall);
        resourcePool()->addResource(videowall);
        return videowall;
    }

    QnVideoWallItem addItem(const QnVideoWallResourcePtr& videowall)
    {
        QnVideoWallItem item;
        item.uuid = QnUuid::createUuid();
        item.runtimeStatus.online = true;
        videowall->items()->addItem(item);
        return item;
    }

    bool enoughLicenses() const
    {
        return m_helper->isValid(Qn::LC_VideoWall);
    }

    // Declares the variables your tests want to use.
    std::unique_ptr<QnStaticCommonModule> m_staticCommon;
    std::unique_ptr<QnCommonModule> m_module;
    QScopedPointer<QnLicensePoolScaffold> m_licenses;
    QScopedPointer<VideoWallLicenseUsageHelper> m_helper;
};

/**
 * License text should be based on the screen count only. All changes in the screens count must be
 * correctly handled.
 */
TEST_F(QnVideowallLicenseUsageHelperTest, validityDependsOnScreensCount)
{
    // Licenses are not required initially.
    ASSERT_TRUE(enoughLicenses());

    // Empty videowall does not require licenses.
    auto videowall = addVideoWall();
    ASSERT_TRUE(enoughLicenses());

    // Adding an item must lead to license recalculation.
    auto item = addItem(videowall);
    ASSERT_FALSE(enoughLicenses());

    // Removing an item must lead to license recalculation.
    videowall->items()->removeItem(item);
    ASSERT_TRUE(enoughLicenses());

    // Adding a videowall must lead to license recalculation.
    auto videowall2 = addVideoWallWithItems(1);
    ASSERT_FALSE(enoughLicenses());

    // Removing a videowall must lead to license recalculation.
    resourcePool()->removeResource(videowall2);
    ASSERT_TRUE(enoughLicenses());
}

} // namespace nx::vms::license::test
