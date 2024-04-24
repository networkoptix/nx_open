// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/license/usage_helper.h>

#include "license_pool_scaffold.h"
#include "license_stub.h"

namespace nx::vms::license::test {

class QnVideowallLicenseUsageHelperTest: public nx::vms::common::test::ContextBasedTest
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_licenses.reset(new QnLicensePoolScaffold(systemContext()->licensePool()));
        m_helper.reset(new VideoWallLicenseUsageHelper(systemContext()));
        m_helper->setCustomValidator(
            std::make_unique<QLicenseStubValidator>(systemContext()));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_helper.reset();
        m_licenses.reset();
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
        item.uuid = nx::Uuid::createUuid();
        item.runtimeStatus.online = true;
        videowall->items()->addItem(item);
        return item;
    }

    bool enoughLicenses() const
    {
        return m_helper->isValid(Qn::LC_VideoWall);
    }

    // Declares the variables your tests want to use.
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

    // Checking that we need 2 license per videowall item.
    addLicenses(1);
    addVideoWallWithItems(1);
    addVideoWallWithItems(1);
    ASSERT_TRUE(enoughLicenses());
    addVideoWallWithItems(1);
    ASSERT_FALSE(enoughLicenses());
}

} // namespace nx::vms::license::test
