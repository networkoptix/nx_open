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

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
    QScopedPointer<QnLicensePoolScaffold> m_licenses;
    QScopedPointer<QnVideoWallLicenseUsageHelper> m_helper;
    QScopedPointer<QLicenseStubValidator> m_validator;
};

TEST_F(QnVideowallLicenseUsageHelperTest, proposeToStartVideowallControl)
{
    QnUuid localInstanceId = QnUuid::createUuid();

    QnVideoWallItem item1;
    item1.uuid = QnUuid::createUuid();

    QnVideoWallItem item2;
    item2.uuid = QnUuid::createUuid();

    auto videowall = addVideoWall();
    videowall->items()->addItem(item1);
    videowall->items()->addItem(item2);

    ASSERT_FALSE(m_helper->isValid(Qn::LC_VideoWall));
    addLicenses(1);

    ASSERT_TRUE(m_helper->isValid(Qn::LC_VideoWall));

    // In the initial state we can start controlling.
    ASSERT_TRUE(m_helper->canStartControlSession(localInstanceId));

    item1.runtimeStatus.online = true;
    item1.runtimeStatus.controlledBy = localInstanceId;
    videowall->items()->updateItem(item1);

    // Another client cannot start control.
    const auto anotherInstanceId = QnUuid::createUuid();
    ASSERT_FALSE(m_helper->canStartControlSession(anotherInstanceId));

    // We can switch our control to another item.
    ASSERT_TRUE(m_helper->canStartControlSession(localInstanceId));
}
