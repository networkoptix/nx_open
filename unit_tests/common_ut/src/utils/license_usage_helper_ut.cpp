#define QN_NO_KEYWORD_UNUSED
#include <gtest/gtest.h>

#include <core/resource_management/resource_pool_scaffold.h>
#include <licensing/license_pool_scaffold.h>

#include <core/resource/camera_resource.h>

#include <utils/license_usage_helper.h>

namespace {
    const int camerasPerAnalogEncoder = QnLicensePool::camerasPerAnalogEncoder();
}

/** Initial test. Check if empty helper is valid. */
TEST( QnCamLicenseUsageHelperTest, init )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    QnCamLicenseUsageHelper helper;
    ASSERT_TRUE( helper.isValid() );
}

/** Basic test for single license type. */
TEST( QnCamLicenseUsageHelperTest, checkSingleLicenseType )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    resPoolScaffold.addCamera();

    QnCamLicenseUsageHelper helper;
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicenses(Qn::LC_Professional, 2);
    ASSERT_TRUE( helper.isValid() );

    resPoolScaffold.addCamera();
    ASSERT_TRUE( helper.isValid() );

    resPoolScaffold.addCamera();
    ASSERT_FALSE( helper.isValid() );
}

/** Basic test for license borrowing. */
TEST( QnCamLicenseUsageHelperTest, borrowTrialLicenses )
{
    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;

    resPoolScaffold.addCamera(Qn::LC_VMAX);
    resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
    resPoolScaffold.addCamera(Qn::LC_Analog);
    resPoolScaffold.addCamera(Qn::LC_Professional);
    resPoolScaffold.addCamera(Qn::LC_Edge);

    ASSERT_FALSE( helper.isValid() );
 
    /* Trial licenses can be used instead of all other types. */
    licPoolScaffold.addLicenses(Qn::LC_Trial, 5);
    ASSERT_TRUE( helper.isValid() );

    resPoolScaffold.addCamera(Qn::LC_VMAX);
    ASSERT_FALSE( helper.isValid() );

    /* Now we should have 5 trial and 1 edge licenses for 2 vmax cameras and 1 of each other type. */
    licPoolScaffold.addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE( helper.isValid() );
}

/** Advanced test for license borrowing. */
TEST( QnCamLicenseUsageHelperTest, borrowTrialAndEdgeLicenses )
{
    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;

    resPoolScaffold.addCameras(Qn::LC_VMAX, 2);
    resPoolScaffold.addCameras(Qn::LC_AnalogEncoder, 2);
    resPoolScaffold.addCameras(Qn::LC_Analog, 2);
    resPoolScaffold.addCameras(Qn::LC_Professional, 2);
    resPoolScaffold.addCameras(Qn::LC_Edge, 2);

    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicenses(Qn::LC_Trial, 5);
    licPoolScaffold.addLicenses(Qn::LC_Edge, 5);
    ASSERT_TRUE( helper.isValid() );
}

/** Advanced test for license borrowing. */
TEST( QnCamLicenseUsageHelperTest, borrowDifferentLicenses )
{
    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;

    /* 14 cameras total. */
    resPoolScaffold.addCameras(Qn::LC_VMAX, 4);
    resPoolScaffold.addCameras(Qn::LC_AnalogEncoder, 4);
    resPoolScaffold.addCameras(Qn::LC_Analog, 3);
    resPoolScaffold.addCameras(Qn::LC_Professional, 2);
    resPoolScaffold.addCameras(Qn::LC_Edge, 1);
    ASSERT_FALSE( helper.isValid() );

    /* Mixed set of licenses that should be enough. */
    licPoolScaffold.addLicenses(Qn::LC_Edge, 2);
    licPoolScaffold.addLicenses(Qn::LC_Professional, 4);
    licPoolScaffold.addLicenses(Qn::LC_Analog, 2);
    licPoolScaffold.addLicenses(Qn::LC_AnalogEncoder, 3);
    licPoolScaffold.addLicenses(Qn::LC_VMAX, 3);
    ASSERT_TRUE( helper.isValid() );
}

/** 
 *  Test for analog encoders politics.
 *  One analog encoder license should allow to record up to camerasPerAnalogEncoder cameras.
 */
TEST( QnCamLicenseUsageHelperTest, checkAnalogEncoderSimple )
{
    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;
    licPoolScaffold.addLicenses(Qn::LC_AnalogEncoder, 1);

    for (int i = 0; i < camerasPerAnalogEncoder; ++i) {
        resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
        ASSERT_TRUE( helper.isValid() ) << "Error at index " << i;
    }
    resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
    ASSERT_FALSE( helper.isValid() );
}

/** 
 *  Test for analog encoders with different group ids.
 *  One license should allow to record up to camerasPerAnalogEncoder cameras 
 *  if and only if they are on the same encoder.
 */
TEST( QnCamLicenseUsageHelperTest, checkAnalogEncoderGroups )
{
    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;
    licPoolScaffold.addLicenses(Qn::LC_AnalogEncoder, 1);

    auto firstEncoder = resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
    firstEncoder->setGroupId(lit("firstEncoder"));
    ASSERT_TRUE( helper.isValid() );

    auto secondEncoder = resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
    secondEncoder->setGroupId(lit("secondEncoder"));
    ASSERT_FALSE( helper.isValid() );
}

/** 
 *  Test for borrowing licenses for analog encoders. 
 *  All cameras that missing the analog encoder license should borrow other licenses one-to-one.
 */
TEST( QnCamLicenseUsageHelperTest, borrowForAnalogEncoder )
{
    int overflow = camerasPerAnalogEncoder - 1;
    if (overflow <= 0)
        return;

    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;
    licPoolScaffold.addLicenses(Qn::LC_AnalogEncoder, 1);

    resPoolScaffold.addCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder + overflow);
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicense(Qn::LC_Professional);
    ASSERT_FALSE( helper.isValid() );

    /* At this moment we will have 1 analog encoder license for camerasPerAnalogEncoder cameras 
       and 2 prof licenses for other cameras. */
    licPoolScaffold.addLicense(Qn::LC_Professional);
    ASSERT_TRUE( helper.isValid() );
}

/** 
 *  Test for calculating required licenses for analog encoders. 
 *  For up to camerasPerAnalogEncoder cameras we should require only 1 analog encoder license.
 */
TEST( QnCamLicenseUsageHelperTest, calculateRequiredLicensesForAnalogEncoder )
{
    QnResourcePoolScaffold resPoolScaffold;
    QnCamLicenseUsageHelper helper;

    for (int i = 0; i < camerasPerAnalogEncoder; ++i) {
        resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
        ASSERT_EQ(helper.requiredLicenses(Qn::LC_AnalogEncoder), 1) << "Error at index " << i;
    }
    resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
    ASSERT_EQ(helper.requiredLicenses(Qn::LC_AnalogEncoder), 2);
}

/** Basic test for single license type proposing. */
TEST( QnCamLicenseUsageHelperTest, proposeSingleLicenseType )
{
    QnResourcePoolScaffold resPoolScaffold;
    auto cameras = resPoolScaffold.addCameras(Qn::LC_Professional, 2, false);

    QnCamLicenseUsageHelper helper;
    QnLicensePoolScaffold licPoolScaffold;
    licPoolScaffold.addLicenses(Qn::LC_Professional, 2);
    
    helper.propose(cameras, true);
    ASSERT_EQ(helper.usedLicenses(Qn::LC_Professional), 2);
    ASSERT_EQ(helper.proposedLicenses(Qn::LC_Professional), 2);
    ASSERT_TRUE( helper.isValid() );   
}

/** Basic test for single license type proposing with borrowing. */
TEST( QnCamLicenseUsageHelperTest, proposeSingleLicenseTypeWithBorrowing )
{
    QnResourcePoolScaffold resPoolScaffold;
    auto cameras = resPoolScaffold.addCameras(Qn::LC_Professional, 2, false);

    QnCamLicenseUsageHelper helper;
    QnLicensePoolScaffold licPoolScaffold;
    licPoolScaffold.addLicenses(Qn::LC_Trial, 2);

    helper.propose(cameras, true);
    ASSERT_EQ(helper.usedLicenses(Qn::LC_Trial), 2);
    ASSERT_EQ(helper.proposedLicenses(Qn::LC_Trial), 2);
    ASSERT_TRUE( helper.isValid() );   
}

/** 
 *  Basic test for analog encoder proposing. 
 *  Valid helper should have correct proposedLicenses count.
 */
TEST( QnCamLicenseUsageHelperTest, proposeAnalogEncoderCameras )
{
    QnResourcePoolScaffold resPoolScaffold;
    auto cameras = resPoolScaffold.addCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder, false);

    QnCamLicenseUsageHelper helper;
    QnLicensePoolScaffold licPoolScaffold;
    licPoolScaffold.addLicenses(Qn::LC_AnalogEncoder, 1);

    helper.propose(cameras, true);
    ASSERT_EQ(helper.usedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_EQ(helper.proposedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_TRUE( helper.isValid() );   
}

/** 
 *  Basic test for analog encoder proposing with overflow. 
 *  Invalid helper should have correct requiredLicenses count.
 */
TEST( QnCamLicenseUsageHelperTest, proposeAnalogEncoderCamerasOverflow )
{
    int overflow = camerasPerAnalogEncoder - 1;
    if (overflow <= 0)
        return;

    QnResourcePoolScaffold resPoolScaffold;
    auto cameras = resPoolScaffold.addCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder + overflow, false);

    QnCamLicenseUsageHelper helper;
    QnLicensePoolScaffold licPoolScaffold;
    licPoolScaffold.addLicenses(Qn::LC_AnalogEncoder, 1);

    helper.propose(cameras, true);
    /* We should see a message like: 2 licenses are used; 1 more license is required */
    ASSERT_EQ(helper.usedLicenses(Qn::LC_AnalogEncoder), 2);    
    ASSERT_EQ(helper.requiredLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_FALSE( helper.isValid() );   
}

/** Basic test for single license type requirement. */
TEST( QnCamLicenseUsageHelperTest, checkRequiredLicenses )
{
    QnResourcePoolScaffold resPoolScaffold;
    auto cameras = resPoolScaffold.addCameras(Qn::LC_Professional, 2, false);

    QnCamLicenseUsageHelper helper;
    QnLicensePoolScaffold licPoolScaffold;

    ASSERT_TRUE( helper.isValid() );   
    helper.propose(cameras, true);
    ASSERT_FALSE( helper.isValid() );   

    ASSERT_EQ(helper.usedLicenses(Qn::LC_Professional), 2);
    ASSERT_EQ(helper.requiredLicenses(Qn::LC_Professional), 2);
}

/** Basic test for analog licenses requirement. */
TEST( QnCamLicenseUsageHelperTest, checkRequiredAnalogLicenses )
{
    QnResourcePoolScaffold resPoolScaffold;
    auto cameras = resPoolScaffold.addCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder, false);

    QnCamLicenseUsageHelper helper;
    QnLicensePoolScaffold licPoolScaffold;

    ASSERT_TRUE( helper.isValid() );   
    helper.propose(cameras, true);
    ASSERT_FALSE( helper.isValid() );   

    ASSERT_EQ(helper.usedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_EQ(helper.requiredLicenses(Qn::LC_AnalogEncoder), 1);
}

/** 
 *  Profiling test.
 */
TEST( QnCamLicenseUsageHelperTest, profileCalculateSpeed )
{
    const int camerasCountPerType = 200;
    const int borrowedCount = 30;   //must be less than half of camerasCountPerType

    QnResourcePoolScaffold resPoolScaffold;
    QnLicensePoolScaffold licPoolScaffold;

    QnCamLicenseUsageHelper helper;

    /* All types of licenses that can be directly required by cameras. */
    auto checkedTypes = QList<Qn::LicenseType>()
        << Qn::LC_Analog
        << Qn::LC_Professional
        << Qn::LC_Edge
        << Qn::LC_VMAX
        << Qn::LC_AnalogEncoder
        << Qn::LC_IO
        ;

    QnVirtualCameraResourceList halfOfCameras;
    
    for (auto t: checkedTypes) {
        resPoolScaffold.addCameras(t, camerasCountPerType / 2, true);
        halfOfCameras.append(
            resPoolScaffold.addCameras(t, camerasCountPerType / 2, false)
        );
    }
  
    for (auto t: checkedTypes) {
        licPoolScaffold.addLicenses(t, camerasCountPerType - borrowedCount);
    }

    ASSERT_TRUE(helper.isValid());
    helper.propose(halfOfCameras, true);
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicenses(Qn::LC_Trial, borrowedCount * Qn::LC_Count);
    ASSERT_TRUE(helper.isValid());
}

/** Basic test for io license type. */
TEST( QnCamLicenseUsageHelperTest, checkIOLicenseType )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    resPoolScaffold.addCamera(Qn::LC_IO, true);

    QnCamLicenseUsageHelper helper;
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicenses(Qn::LC_IO, 1);
    ASSERT_TRUE( helper.isValid() );
}

/** Basic test for start license type. */
TEST( QnCamLicenseUsageHelperTest, checkStartLicenseType )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    resPoolScaffold.addCamera(Qn::LC_Professional, true);

    QnCamLicenseUsageHelper helper;
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicenses(Qn::LC_Start, 1);
    ASSERT_TRUE( helper.isValid() );
}

/** Test for start license borrowing. */
TEST( QnCamLicenseUsageHelperTest, borrowStartLicenses )
{
    QnLicensePoolScaffold licPoolScaffold;
    QnResourcePoolScaffold resPoolScaffold;

    QnCamLicenseUsageHelper helper;

    resPoolScaffold.addCamera(Qn::LC_VMAX);
    resPoolScaffold.addCamera(Qn::LC_AnalogEncoder);
    resPoolScaffold.addCamera(Qn::LC_Analog);
    resPoolScaffold.addCamera(Qn::LC_Professional);

    ASSERT_FALSE( helper.isValid() );

    /* Start licenses can be used instead of some other types. */
    licPoolScaffold.addLicenses(Qn::LC_Start, 10);
    ASSERT_TRUE( helper.isValid() );

    /* Start licenses can't be used instead of edge licenses. */
    resPoolScaffold.addCamera(Qn::LC_Edge);
    ASSERT_FALSE( helper.isValid() );
    licPoolScaffold.addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE( helper.isValid() );

    /* Start licenses can't be used instead of io licenses. */
    resPoolScaffold.addCamera(Qn::LC_IO);
    ASSERT_FALSE( helper.isValid() );
    licPoolScaffold.addLicenses(Qn::LC_IO, 1);
    ASSERT_TRUE( helper.isValid() );
}

/** Test for several start licenses in the same system. */
TEST( QnCamLicenseUsageHelperTest, checkStartLicenseOverlapping )
{
    QnLicensePoolScaffold licPoolScaffold;

    QnResourcePoolScaffold resPoolScaffold;
    resPoolScaffold.addCameras(Qn::LC_Professional, 8, true);

    QnCamLicenseUsageHelper helper;
    ASSERT_FALSE( helper.isValid() );

    /* Two licenses with 6 channels each. */
    licPoolScaffold.addLicenses(Qn::LC_Start, 6);
    licPoolScaffold.addLicenses(Qn::LC_Start, 6);
    ASSERT_FALSE( helper.isValid() );

    /* One licenses with 8 channels. */
    licPoolScaffold.addLicenses(Qn::LC_Start, 8);
    ASSERT_TRUE( helper.isValid() );
}

/** Test for start licenses on the arm servers. */
TEST( QnCamLicenseUsageHelperTest, checkLicensesArmActivating )
{
    /* Nor professional nor starter licenses should not be active on arm server. */
    QnLicensePoolScaffold licPoolScaffold(true);
    licPoolScaffold.addLicenses(Qn::LC_Start, 8);
    licPoolScaffold.addLicenses(Qn::LC_Professional, 8);

    QnResourcePoolScaffold resPoolScaffold;
    resPoolScaffold.addCameras(Qn::LC_Professional, 1, true);

    QnCamLicenseUsageHelper helper;
    ASSERT_FALSE( helper.isValid() );

    licPoolScaffold.addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE( helper.isValid() );
}
