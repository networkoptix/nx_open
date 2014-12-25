#include <gtest/gtest.h>

#include <core/resource_management/resource_pool_scaffold.h>
#include <utils/license_usage_helper.h>

TEST( QnCamLicenseUsageHelperTest, init )
{
    QnResourcePoolScaffold scaffold;
    QnCamLicenseUsageHelper helper;

    ASSERT_EQ( helper.isValid(), true );
}