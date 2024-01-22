// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/license/usage_helper.h>

#include "license_pool_scaffold.h"
#include "license_stub.h"

namespace {
const int camerasPerAnalogEncoder = QnLicensePool::camerasPerAnalogEncoder();
}

enum ServerIndex
{
    primary = 0,
    secondary = 1,
};

namespace nx::vms::license::test {

class QnCamLicenseUsageHelperTest: public nx::vms::common::test::ContextBasedTest
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_servers[0] = addServer();
        m_servers[1] = addServer();
        m_servers[1]->setStatus(nx::vms::api::ResourceStatus::offline);
        m_licenses.reset(new QnLicensePoolScaffold(systemContext()->licensePool()));
        m_helper.reset(new CamLicenseUsageHelper(systemContext()));
        m_helper->setCustomValidator(std::make_unique<QLicenseStubValidator>(systemContext()));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_helper.reset();
        m_licenses.reset();
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
        bool licenseRequired = true,
        ServerIndex serverIndex = ServerIndex::primary)
    {
        QnVirtualCameraResourceList result;
        for (int i = 0; i < count; ++i)
        {
            auto camera = addCamera(licenseType);
            camera->setParentId(m_servers[(int) serverIndex]->getId());
            camera->setScheduleEnabled(licenseRequired);
            result << camera;
        }
        return result;
    }

    QnVirtualCameraResourcePtr addDefaultRecordingCamera()
    {
        // Camera stub should not replace license type.
        const auto kDefaultLicenseType = Qn::LC_Count;
        return addRecordingCamera(kDefaultLicenseType, /* licenseRequired */ false);
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

    std::array<QnMediaServerResourcePtr, 2> m_servers;
    QScopedPointer<QnLicensePoolScaffold> m_licenses;
    QScopedPointer<CamLicenseUsageHelper> m_helper;
};

/** Initial test. Check if empty helper is valid. */
TEST_F(QnCamLicenseUsageHelperTest, init)
{
    ASSERT_TRUE(m_helper->isValid());
}

/** Basic test for single license type. */
TEST_F(QnCamLicenseUsageHelperTest, checkSingleLicenseType)
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
TEST_F(QnCamLicenseUsageHelperTest, borrowTrialLicenses)
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
TEST_F(QnCamLicenseUsageHelperTest, borrowTrialAndEdgeLicenses)
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
TEST_F(QnCamLicenseUsageHelperTest, borrowDifferentLicenses)
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
TEST_F(QnCamLicenseUsageHelperTest, checkAnalogEncoderSimple)
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
TEST_F(QnCamLicenseUsageHelperTest, checkAnalogEncoderGroups)
{
    addLicenses(Qn::LC_AnalogEncoder, 1);

    auto firstEncoder = addRecordingCamera(Qn::LC_AnalogEncoder);
    firstEncoder->setGroupId("firstEncoder");
    ASSERT_TRUE(m_helper->isValid());

    auto secondEncoder = addRecordingCamera(Qn::LC_AnalogEncoder);
    secondEncoder->setGroupId("secondEncoder");
    ASSERT_FALSE(m_helper->isValid());
}

/**
 *  Test for borrowing licenses for analog encoders.
 *  All cameras that missing the analog encoder license should borrow other licenses one-to-one.
 */
TEST_F(QnCamLicenseUsageHelperTest, borrowForAnalogEncoder)
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
TEST_F(QnCamLicenseUsageHelperTest, calculateRequiredLicensesForAnalogEncoder)
{
    for (int i = 0; i < camerasPerAnalogEncoder; ++i)
    {
        addRecordingCamera(Qn::LC_AnalogEncoder);
        ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 1) << "Error at index " << i;
    }
    addRecordingCamera(Qn::LC_AnalogEncoder);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 2);
}

/** Basic test for single license type proposing/unproposing. */
TEST_F(QnCamLicenseUsageHelperTest, proposeSingleLicenseType)
{
    auto cameras = addRecordingCameras(Qn::LC_Professional, 2, false);

    addLicenses(Qn::LC_Professional, 2);

    m_helper->propose(cameras, true);
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Professional), 2);
    ASSERT_EQ(m_helper->proposedLicenses(Qn::LC_Professional), 2);
    ASSERT_TRUE(m_helper->isValid());

    m_helper->propose(QnVirtualCameraResourceList{cameras[0]}, false);
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Professional), 1);
    ASSERT_EQ(m_helper->proposedLicenses(Qn::LC_Professional), 1);
    ASSERT_TRUE(m_helper->isValid());

    m_helper->propose(QnVirtualCameraResourceList{cameras[1]}, false);
    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Professional), 0);
    ASSERT_EQ(m_helper->proposedLicenses(Qn::LC_Professional), 0);
    ASSERT_TRUE(m_helper->isValid());
}

/** Basic test for single license type proposing with borrowing. */
TEST_F(QnCamLicenseUsageHelperTest, proposeSingleLicenseTypeWithBorrowing)
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
TEST_F(QnCamLicenseUsageHelperTest, proposeAnalogEncoderCameras)
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
TEST_F(QnCamLicenseUsageHelperTest, proposeAnalogEncoderCamerasOverflow)
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
TEST_F(QnCamLicenseUsageHelperTest, checkRequiredLicenses)
{
    auto cameras = addRecordingCameras(Qn::LC_Professional, 2, false);

    ASSERT_TRUE(m_helper->isValid());
    m_helper->propose(cameras, true);
    ASSERT_FALSE(m_helper->isValid());

    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_Professional), 2);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_Professional), 2);
}

/** Basic test for analog licenses requirement. */
TEST_F(QnCamLicenseUsageHelperTest, checkRequiredAnalogLicenses)
{
    auto cameras = addRecordingCameras(Qn::LC_AnalogEncoder, camerasPerAnalogEncoder, false);

    ASSERT_TRUE(m_helper->isValid());
    m_helper->propose(cameras, true);
    ASSERT_FALSE(m_helper->isValid());

    ASSERT_EQ(m_helper->usedLicenses(Qn::LC_AnalogEncoder), 1);
    ASSERT_EQ(m_helper->requiredLicenses(Qn::LC_AnalogEncoder), 1);
}

/** Basic test for camera overflow. */
TEST_F(QnCamLicenseUsageHelperTest, checkLicenseOverflow)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, true);

    ASSERT_FALSE(m_helper->isValid());
    ASSERT_TRUE(m_helper->isOverflowForCamera(camera));
}

/** Basic test for camera overflow. */
TEST_F(QnCamLicenseUsageHelperTest, checkLicenseOverflowProposeEnable)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, false);

    ASSERT_TRUE(m_helper->isValid());
    ASSERT_FALSE(m_helper->isOverflowForCamera(camera));

    m_helper->propose(camera, true);

    ASSERT_FALSE(m_helper->isValid());
    ASSERT_TRUE(m_helper->isOverflowForCamera(camera));
}

/** Basic test for camera overflow. */
TEST_F(QnCamLicenseUsageHelperTest, checkLicenseOverflowProposeDisable)
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
TEST_F(QnCamLicenseUsageHelperTest, profileCalculateSpeed)
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
TEST_F(QnCamLicenseUsageHelperTest, ioLicenseCanBeUsedForIoModules)
{
    addRecordingCamera(Qn::LC_IO, true);
    ASSERT_FALSE(m_helper->isValid());
    addLicenses(Qn::LC_IO, 1);
    ASSERT_TRUE(m_helper->isValid());
}

TEST_F(QnCamLicenseUsageHelperTest, ioLicenseCannotBeUsedForCameras)
{
    addRecordingCamera(Qn::LC_Professional, true);
    addLicenses(Qn::LC_IO, 1);
    ASSERT_FALSE(m_helper->isValid());
}

/** Basic test for start license type. */
TEST_F(QnCamLicenseUsageHelperTest, checkStartLicenseType)
{
    addRecordingCamera(Qn::LC_Professional, true);

    ASSERT_FALSE(m_helper->isValid());

    addLicenses(Qn::LC_Start, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Test for start license borrowing. */
TEST_F(QnCamLicenseUsageHelperTest, borrowStartLicenses)
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

/** Test for nvr license borrowing. */
TEST_F(QnCamLicenseUsageHelperTest, borrowNvrLicenses)
{
    addRecordingCamera(Qn::LC_VMAX);
    addRecordingCamera(Qn::LC_AnalogEncoder);
    addRecordingCamera(Qn::LC_Analog);
    addRecordingCamera(Qn::LC_Professional);

    ASSERT_FALSE(m_helper->isValid());

    /* Nvr licenses can be used instead of some other types. */
    addLicenses(Qn::LC_Nvr, 10);
    ASSERT_TRUE(m_helper->isValid());

    /* Nvr licenses can't be used instead of edge licenses. */
    addRecordingCamera(Qn::LC_Edge);
    ASSERT_FALSE(m_helper->isValid());
    addLicenses(Qn::LC_Edge, 1);
    ASSERT_TRUE(m_helper->isValid());

    /* Nvr licenses can't be used instead of io licenses. */
    addRecordingCamera(Qn::LC_IO);
    ASSERT_FALSE(m_helper->isValid());
    addLicenses(Qn::LC_IO, 1);
    ASSERT_TRUE(m_helper->isValid());
}

/** Test for several start licenses in the same system. */
TEST_F(QnCamLicenseUsageHelperTest, checkStartLicenseOverlapping)
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

/** Test for several NVR licenses in the same system. */
TEST_F(QnCamLicenseUsageHelperTest, checkNvrLicenseOverlapping)
{
    addRecordingCameras(Qn::LC_Professional, 8, true);

    ASSERT_FALSE(m_helper->isValid());

    /* Two licenses with 6 channels each. */
    addLicenses(Qn::LC_Nvr, 6);
    addLicenses(Qn::LC_Nvr, 6);
    ASSERT_FALSE(m_helper->isValid());

    /* One licenses with 8 channels. */
    addLicenses(Qn::LC_Nvr, 8);
    ASSERT_TRUE(m_helper->isValid());
}

TEST_F(QnCamLicenseUsageHelperTest, checkVmaxInitLicense)
{
    addLicenses(Qn::LC_VMAX, 1);
    auto vmax = addCamera();

    vmax->setScheduleEnabled(true);
    ASSERT_FALSE(m_helper->isValid());

    vmax->markCameraAsVMax();
    ASSERT_TRUE(m_helper->isValid());
}

TEST_F(QnCamLicenseUsageHelperTest, checkNvrInitLicense)
{
    addLicenses(Qn::LC_Bridge, 1);
    auto nvr = addCamera();

    nvr->setScheduleEnabled(true);
    ASSERT_FALSE(m_helper->isValid());

    nvr->markCameraAsNvr();
    ASSERT_TRUE(m_helper->isValid());
}

/** Check if license info is filled for every license type. */
TEST_F(QnCamLicenseUsageHelperTest, validateLicenseInfo)
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
TEST_F(QnCamLicenseUsageHelperTest, validateLicenseNames)
{
    for (int i = 0; i < Qn::LC_Count; ++i)
    {
        Qn::LicenseType licenseType = static_cast<Qn::LicenseType>(i);

        auto name = QnLicense::displayName(licenseType);
        ASSERT_FALSE(name.isEmpty());

        auto longName = QnLicense::longDisplayName(licenseType);
        ASSERT_FALSE(longName.isEmpty());

        for (int i = 0; i < 10; ++i)
        {
            auto text1 = QnLicense::displayText(licenseType, i);
            ASSERT_FALSE(text1.isEmpty());

            auto text2 = QnLicense::displayText(licenseType, i, 9);
            ASSERT_FALSE(text2.isEmpty());
        }
    }
}

/** Check if license names are filled for every license type. */
TEST_F(QnCamLicenseUsageHelperTest, validateFutureLicenses)
{
    addFutureLicenses(1);
    addRecordingCameras(Qn::LC_Professional, 1, true);
    ASSERT_FALSE(m_helper->isValid());
}

/** Check if can enable recording if everything is good. */
TEST_F(QnCamLicenseUsageHelperTest, canEnableRecordingTrivial)
{
    addLicense(Qn::LC_Professional);
    auto camera = addRecordingCamera(Qn::LC_Professional, false);
    ASSERT_TRUE(m_helper->canEnableRecording(camera));
}

/** Check if can enable recording if recording is already enabled (even if no licenses). */
TEST_F(QnCamLicenseUsageHelperTest, canEnableRecordingIfAlreadyEnabled)
{
    auto camera = addRecordingCamera(Qn::LC_Professional);
    ASSERT_TRUE(m_helper->canEnableRecording(camera));
}

/** Check if cannot enable recording if everything is bad. */
TEST_F(QnCamLicenseUsageHelperTest, canEnableRecordingTrivialNegative)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, false);
    ASSERT_FALSE(m_helper->canEnableRecording(camera));
}

/** Check if can enable recording with license borrowing. */
TEST_F(QnCamLicenseUsageHelperTest, canEnableRecordingBorrowing)
{
    addLicense(Qn::LC_Professional);
    auto camera = addRecordingCamera(Qn::LC_Analog, false);
    ASSERT_TRUE(m_helper->canEnableRecording(camera));
}

/** Check if can enable recording if everything is good on the current camera type. */
TEST_F(QnCamLicenseUsageHelperTest, canEnableRecordingAlreadyInvalid)
{
    auto camera = addRecordingCamera(Qn::LC_Professional);
    addLicense(Qn::LC_Analog);

    auto analogCamera = addRecordingCamera(Qn::LC_Analog, false);
    ASSERT_TRUE(m_helper->canEnableRecording(analogCamera));
}

/** Check if cannot enable recording if something is bad on one of the camera types. */
TEST_F(QnCamLicenseUsageHelperTest, cannotExtendRecordingAlreadyInvalid)
{
    auto camera = addRecordingCamera(Qn::LC_Professional);
    addLicense(Qn::LC_Analog);

    auto analogCamera = addRecordingCamera(Qn::LC_Analog, false);
    auto secondCamera = addRecordingCamera(Qn::LC_Professional, false);
    ASSERT_FALSE(m_helper->canEnableRecording(QnVirtualCameraResourceList()
        << analogCamera << secondCamera));
}

/** Check if cannot enable recording if license is used as borrowed. */
TEST_F(QnCamLicenseUsageHelperTest, cannotSwitchUsedLicense)
{
    auto camera = addRecordingCamera(Qn::LC_Analog);
    addLicense(Qn::LC_Professional);

    auto secondCamera = addRecordingCamera(Qn::LC_Professional, false);
    ASSERT_FALSE(m_helper->canEnableRecording(secondCamera));
}

/** Check if call does not modify inner state. */
TEST_F(QnCamLicenseUsageHelperTest, canEnableRecordingSavesState)
{
    auto camera = addRecordingCamera(Qn::LC_Analog);
    auto secondCamera = addRecordingCamera(Qn::LC_Professional);
    addLicense(Qn::LC_Professional);

    ASSERT_TRUE(m_helper->canEnableRecording(camera));
    ASSERT_TRUE(m_helper->canEnableRecording(secondCamera));
}

TEST_F(QnCamLicenseUsageHelperTest, licenseTypeChanged)
{
    auto camera = addRecordingCamera(Qn::LC_Professional, false);
    ASSERT_FALSE(m_helper->canEnableRecording(camera));

    addLicense(Qn::LC_Professional);
    ASSERT_TRUE(m_helper->canEnableRecording(camera));

    camera.dynamicCast<nx::CameraResourceStub>()->setLicenseType(Qn::LC_Edge);
    ASSERT_FALSE(m_helper->canEnableRecording(camera));
}

TEST_F(QnCamLicenseUsageHelperTest, offlineNeighbor)
{
    // Add a camera to the primary server.
    addLicense(Qn::LC_Professional);
    auto camera = addRecordingCameras(Qn::LC_Professional, 1, true, ServerIndex::primary);

    // Add a camera to the secondary server.
    auto camera2 = addRecordingCameras(Qn::LC_Professional, 1, true, ServerIndex::secondary);

    auto helper = QScopedPointer<CamLicenseUsageHelper>(new CamLicenseUsageHelper(systemContext()));
    helper->setCustomValidator(std::make_unique<QLicenseStubValidator>(systemContext()));
    ASSERT_FALSE(helper->isValid());

    auto helper2 = QScopedPointer<CamLicenseUsageHelper>(new CamLicenseUsageHelper(
        systemContext(), /*considerOnlineServersOnly*/ true));
    helper2->setCustomValidator(std::make_unique<QLicenseStubValidator>(systemContext()));
    ASSERT_TRUE(helper2->isValid());
}

} // namespace nx::vms::license::test
