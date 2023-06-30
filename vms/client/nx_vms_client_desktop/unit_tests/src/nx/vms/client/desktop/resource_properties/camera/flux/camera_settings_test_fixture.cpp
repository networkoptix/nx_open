// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_settings_dialog_state_conversion_functions.h>
#include <nx/vms/client/desktop/resource_properties/camera/watchers/camera_settings_analytics_engines_watcher.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::api;

namespace nx::vms::client::desktop {
namespace test {

struct CameraSettingsAnalyticsEnginesWatcherStub:
    public CameraSettingsAnalyticsEnginesWatcherInterface
{
    virtual QList<core::AnalyticsEngineInfo> engineInfoList() const override
    {
        return analyticsEngines;
    }

    virtual nx::vms::api::StreamIndex analyzedStreamIndex(
        const QnUuid& /*engineId*/) const override
    {
        return nx::vms::api::StreamIndex::undefined;
    }

public:
    QList<core::AnalyticsEngineInfo> analyticsEngines;
};

struct CameraSettingsTestFixture::Private
{

    std::unique_ptr<CameraSettingsAnalyticsEnginesWatcherStub> analyticsEnginesWatcher;
    StubCameraResourceList cameras;

    Private()
    {
        analyticsEnginesWatcher = std::make_unique<CameraSettingsAnalyticsEnginesWatcherStub>();
    }

    QnUuid ensureAnalyticsEngineExistsAndGetId()
    {
        if (analyticsEnginesWatcher->analyticsEngines.empty())
        {
            analyticsEnginesWatcher->analyticsEngines.push_back({
                .id = QnUuid::createUuid(),
                .isDeviceDependent = false});
        }
        return analyticsEnginesWatcher->analyticsEngines.front().id;
    }
};

CameraSettingsTestFixture::CameraSettingsTestFixture():
    d(new Private())
{
}

CameraSettingsTestFixture::~CameraSettingsTestFixture()
{
}

CameraResourceStubPtr CameraSettingsTestFixture::givenCamera(CameraFeatures features)
{
    const auto camera = CameraResourceStubPtr(new CameraResourceStub());
    if (!features.testFlag(CameraFeature::motion))
        camera->setMotionType(MotionType::none);

    if (features.testFlag(CameraFeature::objects))
    {
        const auto id = d->ensureAnalyticsEngineExistsAndGetId();
        camera->setAnalyticsObjectsEnabled(true, id);
        camera->setUserEnabledAnalyticsEngines({id});
    }

    systemContext()->resourcePool()->addResource(camera);
    d->cameras.push_back(camera);

    dispatch(Reducer::loadCameras,
        d->cameras,
        /*deviceAgentSettingsAdapter*/ nullptr,
        d->analyticsEnginesWatcher.get(),
        /*advancedParametersManifestManager*/ nullptr);

    return camera;
}

StubCameraResourceList CameraSettingsTestFixture::cameras() const
{
    return d->cameras;
}

void CameraSettingsTestFixture::whenChangesAreSaved()
{
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state(), d->cameras);
}

void CameraSettingsTestFixture::whenCameraRemoved(CameraResourceStubPtr camera)
{
    d->cameras.removeOne(camera);
    systemContext()->resourcePool()->removeResource(camera);
    dispatch(Reducer::loadCameras,
        d->cameras,
        /*deviceAgentSettingsAdapter*/ nullptr,
        d->analyticsEnginesWatcher.get(),
        /*advancedParametersManifestManager*/ nullptr);
}

void CameraSettingsTestFixture::whenCameraStatusChangedTo(CameraResourceStubPtr camera,
    nx::vms::api::ResourceStatus status)
{
    camera->setStatus(status);
    dispatch(Reducer::handleStatusChanged, d->cameras);
}

void CameraSettingsTestFixture::whenCameraStatusChangedTo(nx::vms::api::ResourceStatus status)
{
    for (auto camera: d->cameras)
        whenCameraStatusChangedTo(camera, status);
}

void CameraSettingsTestFixture::whenCameraFeaturesEnabled(
    CameraResourceStubPtr camera,
    CameraFeatures features,
    bool on)
{
    if (features.testFlag(CameraFeature::motion))
    {
        if (on)
            camera->setMotionType(MotionType::default_);
        else
            camera->setMotionType(MotionType::none);

        dispatch(Reducer::handleMotionTypeChanged, d->cameras);
    }

    if (features.testFlag(CameraFeature::objects))
    {
        if (on)
        {
            const auto id = d->ensureAnalyticsEngineExistsAndGetId();
            camera->setAnalyticsObjectsEnabled(true, id);
        }
        else
        {
            camera->setAnalyticsObjectsEnabled(false);
        }

        dispatch(Reducer::handleCompatibleObjectTypesMaybeChanged, d->cameras);
    }
}

void CameraSettingsTestFixture::whenCameraFeaturesEnabled(CameraFeatures features, bool on)
{
    for (auto camera: d->cameras)
        whenCameraFeaturesEnabled(camera, features, on);
}

void CameraSettingsTestFixture::whenRecordingEnabled(bool on)
{
    dispatch(Reducer::setRecordingEnabled, on);
}

void CameraSettingsTestFixture::whenDualStreamingEnabled(bool on)
{
    dispatch(Reducer::setDualStreamingDisabled, !on);
}

void CameraSettingsTestFixture::whenSecondaryStreamRecordingEnabled(bool on)
{
    dispatch(Reducer::setSecondaryRecordingDisabled, !on);
}

void CameraSettingsTestFixture::whenCamerasAreLoaded()
{
    dispatch(Reducer::loadCameras,
        d->cameras,
        /*deviceAgentSettingsAdapter*/ nullptr,
        d->analyticsEnginesWatcher.get(),
        /*advancedParametersManifestManager*/ nullptr);
}

//-------------------------------------------------------------------------------------------------
// Consistency unit tests.

TEST_F(CameraSettingsTestFixture, givenSingleCameraSupportsMotionAndObjects)
{
    static const std::vector<CameraFeatures> kPossibleFeatureSets{
        CameraFeature::none,
        CameraFeature::motion,
        CameraFeature::objects,
        CameraFeatures(CameraFeature::motion) | CameraFeature::objects
    };

    for (CameraFeatures features: kPossibleFeatureSets)
    {
        auto camera = givenCamera(features);
        EXPECT_EQ(state().isMotionDetectionEnabled(), features.testFlag(CameraFeature::motion));
        EXPECT_EQ(state().isObjectDetectionSupported(), features.testFlag(CameraFeature::objects));
        whenCameraRemoved(camera);
    }
}

TEST_F(CameraSettingsTestFixture, switchingMotion)
{
    givenCamera();
    EXPECT_TRUE(state().isMotionDetectionEnabled());

    whenCameraFeaturesEnabled(CameraFeature::motion, false);
    EXPECT_FALSE(state().isMotionDetectionEnabled());

    whenCameraFeaturesEnabled(CameraFeature::motion);
    EXPECT_TRUE(state().isMotionDetectionEnabled());
}

TEST_F(CameraSettingsTestFixture, switchingObjects)
{
    givenCamera();
    EXPECT_TRUE(state().isObjectDetectionSupported());

    whenCameraFeaturesEnabled(CameraFeature::objects, false);
    EXPECT_FALSE(state().isObjectDetectionSupported());

    whenCameraFeaturesEnabled(CameraFeature::objects);
    EXPECT_TRUE(state().isObjectDetectionSupported());
}

} // namespace test
} // namespace nx::vms::client::desktop
