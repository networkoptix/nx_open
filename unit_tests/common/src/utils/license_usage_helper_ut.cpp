#include <gtest/gtest.h>

#include <core/resource_management/resource_pool_scaffold.h>
#include <licensing/license_pool_scaffold.h>

#include <core/resource/camera_resource.h>

#include <utils/license_usage_helper.h>

TEST( QnCamLicenseUsageHelperTest, init )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    QnCamLicenseUsageHelper helper;
    ASSERT_TRUE( helper.isValid() );
}

TEST( QnCamLicenseUsageHelperTest, testProfessionalLicenses )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    resPoolScaffold.addCamera()->setScheduleDisabled(false);

    QnCamLicenseUsageHelper helper;
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicense(Qn::LC_Professional, 2);
    ASSERT_TRUE( helper.isValid() );

    resPoolScaffold.addCamera()->setScheduleDisabled(false);
    ASSERT_TRUE( helper.isValid() );

    resPoolScaffold.addCamera()->setScheduleDisabled(false);
    ASSERT_FALSE( helper.isValid() );
}
