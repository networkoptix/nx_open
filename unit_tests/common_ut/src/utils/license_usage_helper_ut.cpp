#include <gtest/gtest.h>

#include <common/common_module.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <licensing/license_pool_scaffold.h>
#include <licensing/license_stub.h>

#include <utils/license_usage_helper.h>

namespace {
const int camerasPerAnalogEncoder = QnLicensePool::camerasPerAnalogEncoder();
}

class QnLicenseUsageHelperTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
        initializeContext(m_module.data());
        m_server = addServer();
        m_server->setStatus(Qn::Online);
        m_licenses.reset(new QnLicensePoolScaffold(licensePool()));
        m_helper.reset(new QnCamLicenseUsageHelper(commonModule()));
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

    QnVirtualCameraResourcePtr addRecordingCamera(
        Qn::LicenseType cameraType = Qn::LC_Professional,
        bool licenseRequired = true)
    {
        return addRecordingCameras(cameraType, 1, licenseRequired).first();
    }

    QnVirtualCameraResourceList addRecordingCameras(
        Qn::LicenseType licenseType = Qn::LC_Professional,
        int count = 1,
        bool licenseRequired = true)
    {
        QnVirtualCameraResourceList result;
        for (int i = 0; i < count; ++i)
        {
            auto camera = addCamera(licenseType);
            camera->setParentId(m_server->getId());
            camera->setLicenseUsed(licenseRequired);
            result << camera;
        }
        return result;
    }

    void setArmMode(bool isArm = true)
    {
        m_licenses->setArmMode(isArm);
    }

    void addLicense(Qn::LicenseType licenseType)
    {
        m_licenses->addLicense(licenseType);
    }

    void addLicenses(Qn::LicenseType licenseType, int count)
    {
        m_licenses->addLicenses(licenseType, count);
    }

    void addFutureLicenses(int count)
    {
        m_licenses->addFutureLicenses(count);
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
    QnMediaServerResourcePtr m_server;
    QScopedPointer<QnLicensePoolScaffold> m_licenses;
    QScopedPointer<QnCamLicenseUsageHelper> m_helper;
    QScopedPointer<QLicenseStubValidator> m_validator;
};

/** Initial test. Check if empty helper is valid. */
TEST_F(QnLicenseUsageHelperTest, init)
{
    ASSERT_TRUE(m_helper->isValid());
}

/** Basic test for single license type. */
TEST_F(QnLicenseUsageHelperTest, checkSingleLicenseType)
{
    addRecordingCamera();

    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_Professional, 2);
    ASSERT_TRUE(m_helper->isValid());

    addRecordingCamera();
    ASSERT_TRUE(m_helper->isValid());

    addRecordingCamera();
    ASSERT_FALSE(m_helper->isValid());
}

/** Basic test for license borrowing. */
TEST_F(QnLicenseUsageHelperTest, borrowTrialLicenses)
{
    addRecordingCamera(Qn::LC_VMAX);
    addRecordingCamera(Qn::LC_AnalogEncoder);
    addRecordingCamera(Qn::LC_Analog);
    addRecordingCamera(Qn::LC_Professional);
    addRecordingCamera(Qn::LC_Edge);

    ASSERT_FALSE(m_helper->isValid());

    /* Trial licenses can be used instead of all other types. */
    addLicenses(Qn::LC_Trial, 5);
    ASSERT_TRUE(m_helper->isValid());

    addRecordingCamera(Qn::LC_VMAX);
    ASSERT_FALSE(m_helper->isValid());

    /* Now we should have 5 trial and 1 edge licenses for 2 vmax cameras and 1 of each other type. */
    addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Advanced test for license borrowing. */
TEST_F(QnLicenseUsageHelperTest, borrowTrialAndEdgeLicenses)
{
    addRecordingCameras(Qn::LC_VMAX, 2);
    addRecordingCameras(Qn::LC_AnalogEncoder, 2);
    addRecordingCameras(Qn::LC_Analog, 2);
    addRecordingCameras(Qn::LC_Professional, 2);
    addRecordingCameras(Qn::LC_Edge, 2);

    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_Trial, 5);
    addLicenses(Qn::LC_Edge, 5);
    ASSERT_TRUE(m_helper->isValid());
}

/** Advanced test for license borrowing. */
TEST_F(QnLicenseUsageHelperTest, borrowDifferentLicenses)
{
    /* 14 cameras total. */
    addRecordingCameras(Qn::LC_VMAX, 4);
    addRecordingCameras(Qn::LC_AnalogEncoder, 4);
    addRecordingCameras(Qn::LC_Analog, 3);
    addRecordingCameras(Qn::LC_Professional, 2);
    addRecordingCameras(Qn::LC_Edge, 1);
    ASSERT_FALSE(m_helper->isValid());

    /* Mixed set of licenses that should be enough. */
    addLicenses(Qn::LC_Edge, 2);
    addLicenses(Qn::LC_Professional, 4);
    addLicenses(Qn::LC_Analog, 2);
    addLicenses(Qn::LC_AnalogEncoder, 3);
    addLicenses(Qn::LC_VMAX, 3);
    ASSERT_TRUE(m_helper->isValid());
}

/**
 *  Test for analog encoders politics.
 *  One analog encoder license should allow to record up to camerasPerAnalogEncoder cameras.
 */
TEST_F(QnLicenseUsageHelperTest, checkAnalogEncoderSimple)
{
    addLicenses(Qn::LC_AnalogEncoder, 1);

    for (int i = 0; i < camerasPerAnalogEncoder; ++i)
    {
        addRecordingCamera(Qn::LC_AnalogEncoder);
        ASSERT_TRUE(m_helper->isValid()) << "Error at index " << i;
    }
    addRecordingCamera(Qn::LC_AnalogEncoder);
    ASSERT_FALSE(m_helper->isValid());
}

/**
 *  Test for analog encoders with different group ids.
 *  One license should allow to record up to camerasPerAnalogEncoder cameras
 *  if and only if they are on the same encoder.
 */
TEST_F(QnLicenseUsageHelperTest, checkAnalogEncoderGroups)
{
    addLicenses(Qn::LC_AnalogEncoder, 1);

    auto firstEncoder = addRecordingCamera(Qn::LC_AnalogEncoder);
    firstEncoder->setGroupId(lit("firstEncoder"));
    ASSERT_TRUE(m_helper->isValid());

    auto secondEncoder = addRecordingCamera(Qn::LC_AnalogEncoder);
    secondEncoder->setGroupId(lit("secondEncoder"));
    ASSERT_FALSE(m_helper->isValid());
}

/**
 *  Test for borrowing licenses for analog encoders.
 *  All cameras that missing the analog encoder license should borrow other licenses one-to-one.
 */
TEST_F(QnLicenseUsageHelperTest, borrowForAnalogEncoder)
{
    int overflow = camerasPerAnalogEncoder - 1;
    if (overflow <= 0)
        return;

    addLicenses(Qn::LC_AnalogEncoder, 1);

    addRecordingCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder + overflow);
    ASSERT_FALSE(m_helper->isValid());

    addLicense(Qn::LC_Professional);
    ASSERT_FALSE(m_helper->isValid());

    /* At this moment we will have 1 analog encoder license for camerasPerAnalogEncoder cameras
       and 2 prof licenses for other cameras. */
    addLicense(Qn::LC_Professional);
    ASSERT_TRUE(m_helper->isValid());
}

/**
 *  Test for calculating required licenses for analog encoders.
 *  For up to camerasPerAnalogEncoder cameras we should require only 1 analog encoder license.
 */
TEST_F(QnLicenseUsageHelperTest, calculateRequiredLicensesForAnalogEncoder)
{
    for (int i = 0; i < camerasPerAnalogEncoder; ++i)
    {
        addRecordingCamera(Qn::LC_AnalogEncoder);
        ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 1) << "Error at index " << i;
    }
    addRecordingCamera(Qn::LC_AnalogEncoder);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 2);
}

/** Basic test for single license type proposing. */
TEST_F(QnLicenseUsageHelperTest, proposeSingleLicenseType)
{
    auto cameras = addRecordingCameras(Qn::LC_Professional, 2, false);

    addLicenses(Qn::LC_Professional, 2);

    m_helper->propose(cameras, true);
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Professional), 2);
    ASSERT_EQ(m_helper->proposedLicenses(Qn::LC_Professional), 2);
    ASSERT_TRUE(m_helper->isValid());
}

/** Basic test for single license type proposing with borrowing. */
TEST_F(QnLicenseUsageHelperTest, proposeSingleLicenseTypeWithBorrowing)
{
    auto cameras = addRecordingCameras(Qn::LC_Professional, 2, false);

    addLicenses(Qn::LC_Trial, 2);

    m_helper->propose(cameras, true);
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Trial), 2);
    ASSERT_EQ(m_helper->proposedLicenses(Qn::LC_Trial), 2);
    ASSERT_TRUE(m_helper->isValid());
}

/**
 *  Basic test for analog encoder proposing.
 *  Valid helper should have correct proposedLicenses count.
 */
TEST_F(QnLicenseUsageHelperTest, proposeAnalogEncoderCameras)
{
    auto cameras = addRecordingCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder, false);

    addLicenses(Qn::LC_AnalogEncoder, 1);

    m_helper->propose(cameras, true);
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_EQ(m_helper->proposedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_TRUE(m_helper->isValid());
}

/**
 *  Basic test for analog encoder proposing with overflow.
 *  Invalid helper should have correct requiredLicenses count.
 */
TEST_F(QnLicenseUsageHelperTest, proposeAnalogEncoderCamerasOverflow)
{
    int overflow = camerasPerAnalogEncoder - 1;
    if (overflow <= 0)
        return;

    auto cameras = addRecordingCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder + overflow, false);

    addLicenses(Qn::LC_AnalogEncoder, 1);

    m_helper->propose(cameras, true);
    /* We should see a message like: 2 licenses are used; 1 more license is required */
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_AnalogEncoder), 2);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_FALSE(m_helper->isValid());
}

/** Basic test for single license type requirement. */
TEST_F(QnLicenseUsageHelperTest, checkRequiredLicenses)
{
    auto cameras = addRecordingCameras(Qn::LC_Professional, 2, false);

    ASSERT_TRUE(m_helper->isValid());
    m_helper->propose(cameras, true);
    ASSERT_FALSE(m_helper->isValid());

    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Professional), 2);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_Professional), 2);
}

/** Basic test for analog licenses requirement. */
TEST_F(QnLicenseUsageHelperTest, checkRequiredAnalogLicenses)
{
    auto cameras = addRecordingCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder, false);

    ASSERT_TRUE(m_helper->isValid());
    m_helper->propose(cameras, true);
    ASSERT_FALSE(m_helper->isValid());

    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 1);
}

/** Basic test for camera overflow. */
TEST_F(QnLicenseUsageHelperTest, checkLicenseOverflow)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, true);

    ASSERT_FALSE(m_helper->isValid());
    ASSERT_TRUE(m_helper->isOverflowForCamera(camera));
}

/** Basic test for camera overflow. */
TEST_F(QnLicenseUsageHelperTest, checkLicenseOverflowProposeEnable)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, false);

    ASSERT_TRUE(m_helper->isValid());
    ASSERT_FALSE(m_helper->isOverflowForCamera(camera));

    m_helper->propose(camera, true);

    ASSERT_FALSE(m_helper->isValid());
    ASSERT_TRUE(m_helper->isOverflowForCamera(camera));
}

/** Basic test for camera overflow. */
TEST_F(QnLicenseUsageHelperTest, checkLicenseOverflowProposeDisable)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, true);

    ASSERT_FALSE(m_helper->isValid());
    ASSERT_TRUE(m_helper->isOverflowForCamera(camera));

    m_helper->propose(camera, false);

    ASSERT_TRUE(m_helper->isValid());
    ASSERT_FALSE(m_helper->isOverflowForCamera(camera));
}


/**
 *  Profiling test.
 */
TEST_F(QnLicenseUsageHelperTest, profileCalculateSpeed)
{
    const int camerasCountPerType = 200;
    const int borrowedCount = 30;   //must be less than half of camerasCountPerType

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

    for (auto t : checkedTypes)
    {
        addRecordingCameras(t, camerasCountPerType / 2, true);
        halfOfCameras.append(
            addRecordingCameras(t, camerasCountPerType / 2, false)
        );
    }

    for (auto t : checkedTypes)
        addLicenses(t, camerasCountPerType - borrowedCount);

    ASSERT_TRUE(m_helper->isValid());
    m_helper->propose(halfOfCameras, true);
    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_Trial, borrowedCount * Qn::LC_Count);
    ASSERT_TRUE(m_helper->isValid());
}

/** Basic test for io license type. */
TEST_F(QnLicenseUsageHelperTest, checkIOLicenseType)
{
    addRecordingCamera(Qn::LC_IO, true);

    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_IO, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Basic test for start license type. */
TEST_F(QnLicenseUsageHelperTest, checkStartLicenseType)
{
    addRecordingCamera(Qn::LC_Professional, true);

    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_Start, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Test for start license borrowing. */
TEST_F(QnLicenseUsageHelperTest, borrowStartLicenses)
{
    addRecordingCamera(Qn::LC_VMAX);
    addRecordingCamera(Qn::LC_AnalogEncoder);
    addRecordingCamera(Qn::LC_Analog);
    addRecordingCamera(Qn::LC_Professional);

    ASSERT_FALSE(m_helper->isValid());

    /* Start licenses can be used instead of some other types. */
    addLicenses(Qn::LC_Start, 10);
    ASSERT_TRUE(m_helper->isValid());

    /* Start licenses can't be used instead of edge licenses. */
    addRecordingCamera(Qn::LC_Edge);
    ASSERT_FALSE(m_helper->isValid());
    addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE(m_helper->isValid());

    /* Start licenses can't be used instead of io licenses. */
    addRecordingCamera(Qn::LC_IO);
    ASSERT_FALSE(m_helper->isValid());
    addLicenses(Qn::LC_IO, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Test for several start licenses in the same system. */
TEST_F(QnLicenseUsageHelperTest, checkStartLicenseOverlapping)
{
    addRecordingCameras(Qn::LC_Professional, 8, true);

    ASSERT_FALSE(m_helper->isValid());

    /* Two licenses with 6 channels each. */
    addLicenses(Qn::LC_Start, 6);
    addLicenses(Qn::LC_Start, 6);
    ASSERT_FALSE(m_helper->isValid());

    /* One licenses with 8 channels. */
    addLicenses(Qn::LC_Start, 8);
    ASSERT_TRUE(m_helper->isValid());
}

/** Test for start licenses on the arm servers. */
TEST_F(QnLicenseUsageHelperTest, checkStartLicensesArmActivating)
{
    /* Nor professional nor starter licenses should not be active on arm server. */
    setArmMode();
    addLicenses(Qn::LC_Start, 8);
    addLicenses(Qn::LC_Professional, 8);

    addRecordingCameras(Qn::LC_Professional, 1, true);

    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Test for io licenses on the arm servers. */
TEST_F(QnLicenseUsageHelperTest, checkIoLicensesArmActivating)
{
    setArmMode();
    addLicenses(Qn::LC_IO, 8);
    addRecordingCameras(Qn::LC_IO, 8, true);
    ASSERT_TRUE(m_helper->isValid());
}

TEST_F(QnLicenseUsageHelperTest, checkVmaxInitLicense)
{
    addLicenses(Qn::LC_VMAX, 1);
    auto vmax = addCamera();

    vmax->setLicenseUsed(true);
    ASSERT_FALSE(m_helper->isValid());

    vmax->markCameraAsVMax();
    ASSERT_TRUE(m_helper->isValid());
}

TEST_F(QnLicenseUsageHelperTest, checkNvrInitLicense)
{
    addLicenses(Qn::LC_Bridge, 1);
    auto nvr = addCamera();

    nvr->setLicenseUsed(true);
    ASSERT_FALSE(m_helper->isValid());

    nvr->markCameraAsNvr();
    ASSERT_TRUE(m_helper->isValid());
}

/** Check if license info is filled for every license type. */
TEST_F(QnLicenseUsageHelperTest, validateLicenseInfo)
{
    for (int i = 0; i < Qn::LC_Count; ++i)
    {
        Qn::LicenseType licenseType = static_cast<Qn::LicenseType>(i);

        auto info = QnLicense::licenseTypeInfo(licenseType);
        ASSERT_EQ(licenseType, info.licenseType);

        /* Class name must be in lowercase. */
        ASSERT_EQ(info.className.toLower(), info.className);

        /* Make sure all class names are different. */
        for (int j = 0; j < i; ++j)
            ASSERT_NE(info.className, QnLicense::licenseTypeInfo(static_cast<Qn::LicenseType>(j)).className);

    }
}

/** Check if license names are filled for every license type. */
TEST_F(QnLicenseUsageHelperTest, validateLicenseNames)
{
    for (int i = 0; i < Qn::LC_Count; ++i)
    {
        Qn::LicenseType licenseType = static_cast<Qn::LicenseType>(i);

        auto name = QnLicense::displayName(licenseType);
        ASSERT_FALSE(name.isEmpty());

        auto longName = QnLicense::longDisplayName(licenseType);
        ASSERT_FALSE(longName.isEmpty());
    }
}

/** Check if license names are filled for every license type. */
TEST_F(QnLicenseUsageHelperTest, validateFutureLicenses)
{
    addFutureLicenses(1);
    addRecordingCameras(Qn::LC_Professional, 1, true);
    ASSERT_FALSE(m_helper->isValid());
}
