#include <gtest/gtest.h>

#include <common/common_module.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/videowall_resource.h>

#include <licensing/license_pool_scaffold.h>
#include <licensing/license_stub.h>

#include <utils/license_usage_helper.h>

class QnVideowallLicenseUsageHelperTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
        initializeContext(m_module.data());

        m_licenses.reset(new QnLicensePoolScaffold(licensePool()));
        m_helper.reset(new QnVideoWallLicenseUsageHelper(commonModule()));
        m_validator.reset(new QLicenseStubValidator(commonModule()));
        m_helper->setCustomValidator(m_validator.data());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        m_validator.reset();
        m_helper.reset();
        m_licenses.reset();
        m_module.clear();
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

    bool canStartControlSession(const QnUuid& instanceId) const
    {
        return m_helper->canStartControlSession(instanceId);
    }

    void startControlSession(
        const QnVideoWallResourcePtr& videowall,
        QnVideoWallItem item,
        QnUuid instanceId)
    {
        item.runtimeStatus.controlledBy = instanceId;
        videowall->items()->updateItem(item);
    }

    bool enoughLicenses() const
    {
        return m_helper->isValid(Qn::LC_VideoWall);
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
    QScopedPointer<QnLicensePoolScaffold> m_licenses;
    QScopedPointer<QnVideoWallLicenseUsageHelper> m_helper;
    QScopedPointer<QLicenseStubValidator> m_validator;
};

/**
 * Validate scenarios when user can and cannot start a control session.
 */
TEST_F(QnVideowallLicenseUsageHelperTest, proposeToStartVideowallControl)
{
    auto videowall = addVideoWall();
    auto item = addItem(videowall);

    addLicenses(1);

    // In the initial state we can start controlling.
    const auto localInstanceId = QnUuid::createUuid();
    ASSERT_TRUE(canStartControlSession(localInstanceId));

    startControlSession(videowall, item, localInstanceId);

    // Another client cannot start control.
    const auto anotherInstanceId = QnUuid::createUuid();
    ASSERT_FALSE(canStartControlSession(anotherInstanceId));

    // We can switch our control to another item.
    ASSERT_TRUE(canStartControlSession(localInstanceId));
}

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
